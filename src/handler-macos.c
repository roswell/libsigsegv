/* Fault handler information.  MacOSX version.
   Copyright (C) 1993-1999, 2002-2003  Bruno Haible <bruno@clisp.org>
   Copyright (C) 2003  Paolo Bonzini <bonzini@gnu.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "sigsegv.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#if HAVE_SYS_SIGNAL_H
# include <sys/signal.h>
#endif

#include <mach/vm_map.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/thread_status.h>
#include <mach/exception.h>
#include <mach/task.h>
#include <pthread.h>

/* For MacOSX.  */
#ifndef SS_DISABLE
#define SS_DISABLE SA_DISABLE
#endif

#include "machfault.h"

/* The following sources were used as a *reference* for this exception handling
   code:
      1. Apple's mach/xnu documentation
      2. Timothy J. Wood's "Mach Exception Handlers 101" post to the
         omnigroup's macosx-dev list.
         www.omnigroup.com/mailman/archive/macosx-dev/2000-June/002030.html
      3. macosx-nat.c from Apple's GDB source code.
*/

/* This is not defined in any header, although documented.  */

/* http://web.mit.edu/darwin/src/modules/xnu/osfmk/man/exc_server.html says:
   The exc_server function is the MIG generated server handling function
   to handle messages from the kernel relating to the occurrence of an
   exception in a thread. Such messages are delivered to the exception port
   set via thread_set_exception_ports or task_set_exception_ports. When an
   exception occurs in a thread, the thread sends an exception message to its
   exception port, blocking in the kernel waiting for the receipt of a reply.
   The exc_server function performs all necessary argument handling for this
   kernel message and calls catch_exception_raise, catch_exception_raise_state
   or catch_exception_raise_state_identity, which should handle the exception.
   If the called routine returns KERN_SUCCESS, a reply message will be sent,
   allowing the thread to continue from the point of the exception; otherwise,
   no reply message is sent and the called routine must have dealt with the
   exception thread directly.  */
extern boolean_t
       exc_server (mach_msg_header_t *request_msg,
                   mach_msg_header_t *reply_msg);

/* ?? */
extern kern_return_t
       exception_raise (mach_port_t exception_port,
                        mach_port_t thread,
                        mach_port_t task,
                        exception_type_t exception,
                        exception_data_t code,
                        mach_msg_type_number_t code_count);
extern kern_return_t
       exception_raise_state (mach_port_t exception_port,
                              mach_port_t thread,
                              mach_port_t task,
                              exception_type_t exception,
                              exception_data_t code,
                              mach_msg_type_number_t code_count,
                              thread_state_flavor_t *flavor,
                              thread_state_t in_state,
                              mach_msg_type_number_t in_state_count,
                              thread_state_t out_state,
                              mach_msg_type_number_t *out_state_count);
extern kern_return_t
       exception_raise_state_identity (mach_port_t exception_port,
                                       mach_port_t thread,
                                       mach_port_t task,
                                       exception_type_t exception,
                                       exception_data_t code,
                                       mach_msg_type_number_t code_count,
                                       thread_state_flavor_t *flavor,
                                       thread_state_t in_state,
                                       mach_msg_type_number_t in_state_count,
                                       thread_state_t out_state,
                                       mach_msg_type_number_t *out_state_count);

/* http://web.mit.edu/darwin/src/modules/xnu/osfmk/man/catch_exception_raise.html
   These functions are defined in this file, and called by exc_server.
   FIXME: What needs to be done when this code is put into a shared library? */
kern_return_t
catch_exception_raise (mach_port_t exception_port,
                       mach_port_t thread,
                       mach_port_t task,
                       exception_type_t exception,
                       exception_data_t code,
                       mach_msg_type_number_t code_count);
kern_return_t
catch_exception_raise_state (mach_port_t exception_port,
                             exception_type_t exception,
                             exception_data_t code,
                             mach_msg_type_number_t code_count,
                             thread_state_flavor_t *flavor,
                             thread_state_t in_state,
                             mach_msg_type_number_t in_state_count,
                             thread_state_t out_state,
                             mach_msg_type_number_t *out_state_count);
kern_return_t
catch_exception_raise_state_identity (mach_port_t exception_port,
                                      mach_port_t thread,
                                      mach_port_t task,
                                      exception_type_t exception,
                                      exception_data_t code,
                                      mach_msg_type_number_t codeCnt,
                                      thread_state_flavor_t *flavor,
                                      thread_state_t in_state,
                                      mach_msg_type_number_t in_state_count,
                                      thread_state_t out_state,
                                      mach_msg_type_number_t *out_state_count);


/* The maximum number of exception port informations to be saved.
   FIXME: Is 1 not sufficient? Or do each of the three functions count as
   a separate exception port?  */
#define MAX_EXCEPTION_PORTS 16

/* The previous exception port informations for the entire task.  */
static struct
{
  mach_msg_type_number_t count;
  exception_mask_t masks[MAX_EXCEPTION_PORTS];
  exception_handler_t ports[MAX_EXCEPTION_PORTS];
  exception_behavior_t behaviors[MAX_EXCEPTION_PORTS];
  thread_state_flavor_t flavors[MAX_EXCEPTION_PORTS];
} old_exc_ports;

/* The exception port on which our thread listens.  */
static mach_port_t our_exception_port;

/* User's stack overflow handler.  */
static stackoverflow_handler_t stk_user_handler = (stackoverflow_handler_t)NULL;
static unsigned long stk_extra_stack;
static unsigned long stk_extra_stack_size;

/* User's fault handler.  */
static sigsegv_handler_t user_handler = (sigsegv_handler_t)NULL;

/* Our SIGSEGV handler, for stack overflow.  */
static void
sigsegv_handler (int sig)
{
  static int emergency = -1;

  /* Did the user install a stack overflow handler?  */
  if (stk_user_handler)
    {
      void *context = NULL;
      emergency++;
      /* Call user's handler.  */
      (*stk_user_handler) (emergency, context);
      emergency--;
    }
}


/* Our SIGBUS handler.  Sometimes Darwin calls it despite our exception
   handling thread. (Maybe this is normal?? The thread-specific exception
   ports are notified before the one for the entire task.)  */
static void
sigbus_handler (int sig)
{
}


/* Forward an exception to the previously installed handlers.  */
static kern_return_t
forward_exception (mach_port_t thread,
                   mach_port_t task,
                   exception_type_t exception,
                   exception_data_t code,
                   mach_msg_type_number_t code_count)
{
  int i;

#ifdef DEBUG_EXCEPTION_HANDLING
  fprintf (stderr, "Forwarding exception...\n");
#endif
  /* Find which previously installed handler it should be forwarded to.
     This loop is nonsense since we are registered to handle only
     EXC_BAD_ACCESS, but a little extensibility doesn't hurt.  */
  for (i = 0; i < old_exc_ports.count; i++)
    if (old_exc_ports.masks[i] & (1 << exception))
      break;
  if (i == old_exc_ports.count)
    {
#ifdef DEBUG_EXCEPTION_HANDLING
      fprintf (stderr, "No handler for exception!\n");
#endif
      abort ();
    }

  {
    mach_port_t port = old_exc_ports.ports[i];
    exception_behavior_t behavior = old_exc_ports.behaviors[i];
    thread_state_flavor_t flavor = old_exc_ports.flavors[i];
    kern_return_t retval;

    if (behavior == EXCEPTION_DEFAULT)
      {
#ifdef DEBUG_EXCEPTION_HANDLING
        fprintf (stderr, "Forwarding EXCEPTION_DEFAULT...\n");
#endif
        retval = exception_raise (port, thread, task,
                                  exception, code, code_count);
      }
    else
      {
        thread_state_data_t thread_state;
        mach_msg_type_number_t thread_state_count;

        /* See http://web.mit.edu/darwin/src/modules/xnu/osfmk/man/thread_get_state.html.  */
        thread_state_count = THREAD_STATE_MAX;
        if (thread_get_state (thread, flavor,
                              thread_state, &thread_state_count)
            != KERN_SUCCESS)
          {
#ifdef DEBUG_EXCEPTION_HANDLING
            fprintf (stderr, "thread_get_state failed in forward_exception\n");
#endif
            abort ();
          }

        if (behavior == EXCEPTION_STATE)
          {
#ifdef DEBUG_EXCEPTION_HANDLING
            fprintf (stderr, "Forwarding EXCEPTION_STATE...\n");
#endif
            retval = exception_raise_state (port, thread, task,
                                            exception, code, code_count,
                                            &flavor,
                                            thread_state, thread_state_count,
                                            thread_state, &thread_state_count);
          }
        else if (behavior == EXCEPTION_STATE_IDENTITY)
          {
#ifdef DEBUG_EXCEPTION_HANDLING
            fprintf (stderr, "Forwarding EXCEPTION_STATE_IDENTITY...\n");
#endif
            retval = exception_raise_state_identity (port, thread, task,
                                                     exception, code, code_count,
                                                     &flavor,
                                                     thread_state, thread_state_count,
                                                     thread_state, &thread_state_count);
          }
        else
          {
#ifdef DEBUG_EXCEPTION_HANDLING
            fprintf (stderr, "unknown behavior: %d\n", behavior);
#endif
            abort ();
          }

        /* See http://web.mit.edu/darwin/src/modules/xnu/osfmk/man/thread_set_state.html.  */
        if (thread_set_state (thread, flavor, thread_state, thread_state_count)
            != KERN_SUCCESS)
          {
#ifdef DEBUG_EXCEPTION_HANDLING
            fprintf (stderr, "thread_set_state failed in forward_exception\n");
#endif
            abort ();
          }
      }

    return retval;
  }
}


/* Handle an exception by invoking the user's fault handler and/or forwarding
   the duty to the previously installed handlers.  */
kern_return_t
catch_exception_raise (mach_port_t exception_port,
                       mach_port_t thread,
                       mach_port_t task,
                       exception_type_t exception,
                       exception_data_t code,
                       mach_msg_type_number_t code_count)
{
#ifdef DEBUG_EXCEPTION_HANDLING
  fprintf (stderr, "Exception: 0x%x Code: 0x%x 0x%x in catch....\n",
           exception,
           code_count > 0 ? code[0] : -1,
           code_count > 1 ? code[1] : -1);
#endif

  if (exception == EXC_BAD_ACCESS
      && code[0] == KERN_PROTECTION_FAILURE
      && user_handler)
    {
      SIGSEGV_STATE_TYPE exc_state;
      mach_msg_type_number_t exc_state_count;

      /* See http://web.mit.edu/darwin/src/modules/xnu/osfmk/man/thread_get_state.html.  */
      exc_state_count = SIGSEGV_STATE_COUNT;
      if (thread_get_state (thread, SIGSEGV_STATE_FLAVOR,
                            (void *) &exc_state, &exc_state_count)
          != KERN_SUCCESS)
        {
          /* The thread is supposed to be suspended while the exception handler
             is called. This shouldn't fail. */
#ifdef DEBUG_EXCEPTION_HANDLING
          fprintf (stderr, "thread_get_state failed in catch_exception_raise\n");
#endif
        }
      else
        {
          /* Got the thread's state. Now extract the address that caused the
             fault and invoke the user's handler.  */
          int done;

#ifdef DEBUG_EXCEPTION_HANDLING
          fprintf (stderr, "Calling user handler, addr = 0x%lx\n", (char *) exc_state.dar);
#endif
          done = (*user_handler) ((char *) (SIGSEGV_FAULT_ADDRESS), 0);
#ifdef DEBUG_EXCEPTION_HANDLING
          fprintf (stderr, "Back from user handler\n");
#endif
          if (done)
            return KERN_SUCCESS;
        }
    }

  /* Forward the exception to the previous handler.  */
  forward_exception(thread, task, exception, code, code_count);
}

#if 0 /* Would be called if we registered our_exception_port with
         EXCEPTION_STATE below.  */
kern_return_t
catch_exception_raise_state (mach_port_t exception_port,
                             exception_type_t exception,
                             exception_data_t code,
                             mach_msg_type_number_t code_count,
                             thread_state_flavor_t *flavor,
                             thread_state_t in_state,
                             mach_msg_type_number_t in_state_count,
                             thread_state_t out_state,
                             mach_msg_type_number_t *out_state_count)
{
#ifdef DEBUG_EXCEPTION_HANDLING
  fprintf (stderr, "catch_exception_raise_state\n");
#endif
  /* If this ever gets called, implement it similarly to
     catch_exception_raise().  */
  abort ();
}
#endif

#if 0 /* Would be called if we registered our_exception_port with
         EXCEPTION_STATE_IDENTITY below.  */
kern_return_t
catch_exception_raise_state_identity (mach_port_t exception_port,
                                      mach_port_t thread,
                                      mach_port_t task,
                                      exception_type_t exception,
                                      exception_data_t code,
                                      mach_msg_type_number_t codeCnt,
                                      thread_state_flavor_t *flavor,
                                      thread_state_t in_state,
                                      mach_msg_type_number_t in_state_count,
                                      thread_state_t out_state,
                                      mach_msg_type_number_t *out_state_count)
{
#ifdef DEBUG_EXCEPTION_HANDLING
  fprintf (stderr, "catch_exception_raise_state_identity\n");
#endif
  /* If this ever gets called, implement it similarly to
     catch_exception_raise().  */
  abort ();
}
#endif


/* The main function of the thread listening for exceptions.  */
static void *
mach_exception_thread (void *arg)
{
  for (;;)
    {
      /* These two structures contain some private kernel data. We don't need
         to access any of it so we don't bother defining a proper struct. The
         correct definitions are in the xnu source code. */
      /* Buffer for a message to be received.  */
      struct
        {
          mach_msg_header_t head;
          mach_msg_body_t msgh_body;
          char data[1024];
        }
        msg;
      /* Buffer for a reply message.  */
      struct
        {
          mach_msg_header_t head;
          char data[1024];
        }
        reply;

      mach_msg_return_t retval;

#ifdef DEBUG_EXCEPTION_HANDLING
      fprintf (stderr, "Exception thread going to sleep\n");
#endif

      /* Wait for a message on the exception port.  */
      retval = mach_msg (&msg.head, MACH_RCV_MSG | MACH_RCV_LARGE, 0,
                         sizeof (msg), our_exception_port,
                         MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
#ifdef DEBUG_EXCEPTION_HANDLING
      fprintf (stderr, "Exception thread woke up\n");
#endif
      if (retval != MACH_MSG_SUCCESS)
        {
#ifdef DEBUG_EXCEPTION_HANDLING
          fprintf (stderr, "mach_msg receive failed with %d %s\n",
                   (int) retval, mach_error_string (retval));
#endif
          abort ();
        }

      /* Handle the message: Call exc_server, which will call
         catch_exception_raise and produce a reply message.  */
#ifdef DEBUG_EXCEPTION_HANDLING
      fprintf (stderr, "Calling exc_server\n");
#endif
      exc_server (&msg.head, &reply.head);
#ifdef DEBUG_EXCEPTION_HANDLING
      fprintf (stderr, "Finished exc_server\n");
#endif

      /* Send the reply.  */
      if (mach_msg (&reply.head, MACH_SEND_MSG, reply.head.msgh_size,
                    0, MACH_PORT_NULL,
                    MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL)
          != MACH_MSG_SUCCESS)
        {
#ifdef DEBUG_EXCEPTION_HANDLING
          fprintf (stderr, "mach_msg send failed\n");
#endif
          abort ();
        }
#ifdef DEBUG_EXCEPTION_HANDLING
      fprintf (stderr, "Reply successful\n");
#endif
    }
}


/* Initialize the Mach exception handler thread.
   Return 0 if OK, -1 on error.  */
static int
mach_initialize ()
{
  mach_port_t self;
  exception_mask_t mask;
  pthread_attr_t attr;
  pthread_t thread;

  self = mach_task_self ();

  /* Allocate a port on which the thread shall listen for exceptions.  */
  if (mach_port_allocate (self, MACH_PORT_RIGHT_RECEIVE, &our_exception_port)
      != KERN_SUCCESS)
    return -1;

  /* See http://web.mit.edu/darwin/src/modules/xnu/osfmk/man/mach_port_insert_right.html.  */
  if (mach_port_insert_right (self, our_exception_port, our_exception_port,
                              MACH_MSG_TYPE_MAKE_SEND)
      != KERN_SUCCESS)
    return -1;

  /* The exceptions we want to catch.  Only EXC_BAD_ACCESS is interesting
     for us (see above in function catch_exception_raise).  */
  mask = EXC_MASK_BAD_ACCESS;

  /* Get the exception port info for these exceptions.
     See http://web.mit.edu/darwin/src/modules/xnu/osfmk/man/task_get_exception_ports.html.  */
  if (task_get_exception_ports (self,
                                mask,
                                old_exc_ports.masks,
                                &old_exc_ports.count,
                                old_exc_ports.ports,
                                old_exc_ports.behaviors,
                                old_exc_ports.flavors)
      != KERN_SUCCESS)
    return -1;

  /* Create the thread listening on the exception port.  */
  if (pthread_attr_init (&attr) != 0)
    return -1;
  if (pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED) != 0)
    return -1;
  if (pthread_create (&thread, &attr, mach_exception_thread, NULL) != 0)
    return -1;
  pthread_attr_destroy (&attr);

  /* Replace the exception port info for these exceptions with our own.
     Note that we replace the exception port for the entire task, not only
     for a particular thread.  This has the effect that when our exception
     port gets the message, the thread specific exception port has already
     been asked, and we don't need to bother about it.  */
     See http://web.mit.edu/darwin/src/modules/xnu/osfmk/man/task_set_exception_ports.html.  */
  if (task_set_exception_ports (self,
                                mask,
                                our_exception_port,
                                /* It's probably cheapest to ask for the exception state only
                                   when we need it. So we use EXCEPTION_DEFAULT.  */
                                EXCEPTION_DEFAULT /* -> catch_exception_raise */
                                /* EXCEPTION_STATE -> catch_exception_raise_state */
                                /* EXCEPTION_STATE_IDENTITY -> catch_exception_raise_state_identity */,
                                MACHINE_THREAD_STATE)
      != KERN_SUCCESS)
    return -1;

  return 0;
}


/* From handler-unix.c.
   The handler argument here depends on the signal (we need different
   handlers for SIGSEGV and SIGBUS) and doesn't rely on siginfo (because
   we get the fault address through the thread above).  */
static void
install_for (int sig, void (*handler)(int))
{
  struct sigaction action;

  action.sa_handler = handler;

  /* Block most signals while SIGSEGV is being handled.  */
  /* Signals SIGKILL, SIGSTOP cannot be blocked.  */
  /* Signals SIGCONT, SIGTSTP, SIGTTIN, SIGTTOU are not blocked because
     dealing with these signals seems dangerous.  */
  /* Signals SIGILL, SIGABRT, SIGFPE, SIGSEGV, SIGTRAP, SIGIOT, SIGEMT, SIGBUS,
     SIGSYS, SIGSTKFLT are not blocked because these are synchronous signals,
     which may require immediate intervention, otherwise the process may
     starve.  */
  sigemptyset (&action.sa_mask);
#ifdef SIGHUP
  sigaddset (&action.sa_mask,SIGHUP);
#endif
#ifdef SIGINT
  sigaddset (&action.sa_mask,SIGINT);
#endif
#ifdef SIGQUIT
  sigaddset (&action.sa_mask,SIGQUIT);
#endif
#ifdef SIGPIPE
  sigaddset (&action.sa_mask,SIGPIPE);
#endif
#ifdef SIGALRM
  sigaddset (&action.sa_mask,SIGALRM);
#endif
#ifdef SIGTERM
  sigaddset (&action.sa_mask,SIGTERM);
#endif
#ifdef SIGUSR1
  sigaddset (&action.sa_mask,SIGUSR1);
#endif
#ifdef SIGUSR2
  sigaddset (&action.sa_mask,SIGUSR2);
#endif
#ifdef SIGCHLD
  sigaddset (&action.sa_mask,SIGCHLD);
#endif
#ifdef SIGCLD
  sigaddset (&action.sa_mask,SIGCLD);
#endif
#ifdef SIGURG
  sigaddset (&action.sa_mask,SIGURG);
#endif
#ifdef SIGIO
  sigaddset (&action.sa_mask,SIGIO);
#endif
#ifdef SIGPOLL
  sigaddset (&action.sa_mask,SIGPOLL);
#endif
#ifdef SIGXCPU
  sigaddset (&action.sa_mask,SIGXCPU);
#endif
#ifdef SIGXFSZ
  sigaddset (&action.sa_mask,SIGXFSZ);
#endif
#ifdef SIGVTALRM
  sigaddset (&action.sa_mask,SIGVTALRM);
#endif
#ifdef SIGPROF
  sigaddset (&action.sa_mask,SIGPROF);
#endif
#ifdef SIGPWR
  sigaddset (&action.sa_mask,SIGPWR);
#endif
#ifdef SIGLOST
  sigaddset (&action.sa_mask,SIGLOST);
#endif
#ifdef SIGWINCH
  sigaddset (&action.sa_mask,SIGWINCH);
#endif
  /* Note that sigaction() implicitly adds sig itself to action.sa_mask.  */
  /* Ask the OS to provide a structure siginfo_t to the handler.  */
  action.sa_flags = 0;
  sigaction (sig, &action, (struct sigaction *) NULL);
}

int
sigsegv_install_handler (sigsegv_handler_t handler)
{
  /* mach_initialize() status:
     0: not yet called
     1: called and succeeded
     -1: called and failed  */
  static int mach_initialized = 0;

  if (!mach_initialized)
    mach_initialized = (mach_initialize () >= 0 ? 1 : -1);
  if (mach_initialized < 0)
    return -1;

  user_handler = handler;

  /* Install the signal handlers with the appropriate mask.  */
  install_for (SIGBUS, sigbus_handler);
  return 0;
}

void
sigsegv_deinstall_handler (void)
{
  user_handler = (sigsegv_handler_t)NULL;
  signal (SIGBUS, SIG_DFL);
}

void
sigsegv_leave_handler (void)
{
  sigsegv_reset_onstack_flag ();
}

int
stackoverflow_install_handler (stackoverflow_handler_t handler,
                               void *extra_stack, unsigned long extra_stack_size)
{
  stk_user_handler = handler;
  stk_extra_stack = (unsigned long) extra_stack;
  stk_extra_stack_size = extra_stack_size;
  {
    stack_t ss;
    ss.ss_sp = extra_stack;
    ss.ss_size = extra_stack_size;
    ss.ss_flags = 0; /* no SS_DISABLE */
    if (sigaltstack (&ss, (stack_t*)0) < 0)
      return -1;
  }

  /* Install the signal handlers with the appropriate mask.  */
  install_for (SIGSEGV, sigsegv_handler);
  return 0;
}

void
stackoverflow_deinstall_handler (void)
{
  stk_user_handler = (stackoverflow_handler_t) NULL;
  signal (SIGSEGV, SIG_DFL);

  {
    stack_t ss;
    ss.ss_flags = SS_DISABLE;
    if (sigaltstack (&ss, (stack_t *) 0) < 0)
      perror ("libsigsegv (stackoverflow_deinstall_handler)");
  }
}
