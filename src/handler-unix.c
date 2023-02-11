/* Fault handler information.  Unix version.
   Copyright (C) 1993-1999, 2002-2003, 2006, 2008-2009, 2016-2017, 2021, 2023  Bruno Haible <bruno@clisp.org>

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* Persuade glibc <sys/ucontext.h> to declare macros designating register
   indices: REG_RSP on x86_64, REG_ESP on i386.  */
#define _GNU_SOURCE 1

/* Persuade Solaris OpenIndiana <ucontext.h> to include <sys/regset.h>,
   which declares macros designating register indices, such as ESP on i386.  */
#define __EXTENSIONS__ 1

#include "sigsegv.h"

/* On the average Unix platform, we define

   HAVE_SIGSEGV_RECOVERY
       if there is a fault-*.h include file which defines
       SIGSEGV_FAULT_HANDLER_ARGLIST and SIGSEGV_FAULT_ADDRESS.

   HAVE_STACK_OVERFLOW_RECOVERY
       if HAVE_SIGALTSTACK is set and
       at least two of the following are true:
       A) There is a fault-*.h include file which defines
          SIGSEGV_FAULT_HANDLER_ARGLIST and SIGSEGV_FAULT_ADDRESS.
       B) There is a fault-*.h include file which defines
          SIGSEGV_FAULT_HANDLER_ARGLIST and SIGSEGV_FAULT_STACKPOINTER.
       C) There is a stackvma-*.c, other than stackvma-none.c, which
          defines sigsegv_get_vma.

   Why? Obviously, to catch stack overflow, we need an alternate signal
   stack; this requires kernel support. But we also need to distinguish
   (with a reasonable confidence) a stack overflow from a regular SIGSEGV.
   If we have A) and B), we use the
     Heuristic AB: If the fault address is near the stack pointer, it's a
     stack overflow.
   If we have A) and C), we use the
     Heuristic AC: If the fault address is near and beyond the bottom of
     the stack's virtual memory area, it's a stack overflow.
   If we have B) and C), we use the
     Heuristic BC: If the stack pointer is near the bottom of the stack's
     virtual memory area, it's a stack overflow.
     This heuristic comes in two flavours: On OSes which let the stack's
     VMA grow continuously, we determine the bottom by use of getrlimit().
     On OSes which preallocate the stack's VMA with its maximum size
     (like BeOS), we use the stack's VMA directly.
 */
/* Note: Solaris 11 defines a function 'stack_violation'
   <https://docs.oracle.com/cd/E26502_01/html/E29034/stack-violation-3c.html>.
   It merely implements heuristic BC, therefore no need to use it.  */

#include <stdint.h>
#include <stdlib.h>
#include <signal.h>
#if HAVE_SYS_SIGNAL_H
# include <sys/signal.h>
#endif
#include <errno.h>

/* For MacOSX.  */
#ifndef SS_DISABLE
#define SS_DISABLE SA_DISABLE
#endif

#include "fault.h"
#include CFG_SIGNALS

#if HAVE_STACK_OVERFLOW_RECOVERY

#include <stdio.h> /* perror */

#if HAVE_GETRLIMIT
# include <sys/types.h>
# include <sys/time.h>
# include <sys/resource.h> /* declares struct rlimit */
#endif

/* Platform dependent:
   Determine the virtual memory area of a given address.  */
#include "stackvma.h"

/* Platform dependent:
   Leaving a signal handler executing on the alternate stack.  */
#include "leave.h"

#if HAVE_STACKVMA

/* Address of the last byte belonging to the stack vma.  */
static uintptr_t stack_top = 0;

/* Needs to be called once only.  */
static void
remember_stack_top (void *some_variable_on_stack)
{
  struct vma_struct vma;

  if (sigsegv_get_vma ((uintptr_t) some_variable_on_stack, &vma) >= 0)
    stack_top = vma.end - 1;
}

#endif /* HAVE_STACKVMA */

static stackoverflow_handler_t stk_user_handler = (stackoverflow_handler_t)NULL;
static uintptr_t stk_extra_stack;
static size_t stk_extra_stack_size;

#endif /* HAVE_STACK_OVERFLOW_RECOVERY */

#if HAVE_SIGSEGV_RECOVERY

/* User's SIGSEGV handler.  */
static sigsegv_handler_t user_handler = (sigsegv_handler_t)NULL;

#endif /* HAVE_SIGSEGV_RECOVERY */


/* Our SIGSEGV handler, with OS dependent argument list.  */

#if HAVE_SIGSEGV_RECOVERY

static void
sigsegv_handler (SIGSEGV_FAULT_HANDLER_ARGLIST)
{
  void *address = (void *) (SIGSEGV_FAULT_ADDRESS);

#if HAVE_STACK_OVERFLOW_RECOVERY
#if !(HAVE_STACKVMA || defined SIGSEGV_FAULT_STACKPOINTER)
#error "Insufficient heuristics for detecting a stack overflow.  Either define CFG_STACKVMA and HAVE_STACKVMA correctly, or define SIGSEGV_FAULT_STACKPOINTER correctly, or undefine HAVE_STACK_OVERFLOW_RECOVERY!"
#endif

  /* Call user's handler.  */
  if (user_handler && (*user_handler) (address, 0))
    {
      /* Handler successful.  */
    }
  else
    {
      /* Handler declined responsibility.  */

      /* Did the user install a stack overflow handler?  */
      if (stk_user_handler)
        {
          /* See whether it was a stack overflow. If so, longjump away.  */
#ifdef SIGSEGV_FAULT_STACKPOINTER
          uintptr_t old_sp = (uintptr_t) (SIGSEGV_FAULT_STACKPOINTER);
#ifdef __ia64
          uintptr_t old_bsp = (uintptr_t) (SIGSEGV_FAULT_BSP_POINTER);
#endif
#endif

#if HAVE_STACKVMA
          /* Were we able to determine the stack top?  */
          if (stack_top)
            {
              /* Determine stack bounds.  */
              int saved_errno;
              struct vma_struct vma;
              int ret;

              saved_errno = errno;
              ret = sigsegv_get_vma (stack_top, &vma);
              errno = saved_errno;
              if (ret >= 0)
                {
#ifndef BOGUS_FAULT_ADDRESS_UPON_STACK_OVERFLOW
                  /* Heuristic AC: If the fault_address is nearer to the stack
                     segment's [start,end] than to the previous segment, we
                     consider it a stack overflow.
                     In the case of IA-64, we know that the previous segment
                     is the up-growing bsp segment, and either of the two
                     stacks can overflow.  */
                  uintptr_t addr = (uintptr_t) address;

#ifdef __ia64
                  if (addr >= vma.prev_end && addr <= vma.end - 1)
#else
#if STACK_DIRECTION < 0
                  if (addr >= vma.start
                      ? (addr <= vma.end - 1)
                      : vma.is_near_this (addr, &vma))
#else
                  if (addr <= vma.end - 1
                      ? (addr >= vma.start)
                      : vma.is_near_this (addr, &vma))
#endif
#endif
                    {
#else /* BOGUS_FAULT_ADDRESS_UPON_STACK_OVERFLOW */
#if HAVE_GETRLIMIT && defined RLIMIT_STACK
                  /* Heuristic BC: If the stack size has reached its maximal size,
                     and old_sp is near the low end, we consider it a stack
                     overflow.  */
                  struct rlimit rl;

                  saved_errno = errno;
                  ret = getrlimit (RLIMIT_STACK, &rl);
                  errno = saved_errno;
                  if (ret >= 0)
                    {
                      uintptr_t current_stack_size = vma.end - vma.start;
                      uintptr_t max_stack_size = rl.rlim_cur;
                      if (current_stack_size <= max_stack_size + 4096
                          && max_stack_size <= current_stack_size + 4096
#else
                    {
                      if (1
#endif
#ifdef SIGSEGV_FAULT_STACKPOINTER
                          /* Heuristic BC: If we know old_sp, and it is neither
                             near the low end, nor in the alternate stack, then
                             it's probably not a stack overflow.  */
                          && ((old_sp >= stk_extra_stack
                               && old_sp <= stk_extra_stack + stk_extra_stack_size)
#if STACK_DIRECTION < 0
                              || (old_sp <= vma.start + 4096
                                  && vma.start <= old_sp + 4096))
#else
                              || (old_sp <= vma.end + 4096
                                  && vma.end <= old_sp + 4096))
#endif
#endif
                         )
#endif /* BOGUS_FAULT_ADDRESS_UPON_STACK_OVERFLOW */
#else /* !HAVE_STACKVMA */
          /* Heuristic AB: If the fault address is near the stack pointer,
             it's a stack overflow.  */
          uintptr_t addr = (uintptr_t) address;

          if ((addr <= old_sp + 4096 && old_sp <= addr + 4096)
#ifdef __ia64
              || (addr <= old_bsp + 4096 && old_bsp <= addr + 4096)
#endif
             )
            {
                {
                    {
#endif /* !HAVE_STACKVMA */
                        {
#ifdef SIGSEGV_FAULT_STACKPOINTER
                          int emergency =
                            (old_sp >= stk_extra_stack
                             && old_sp <= stk_extra_stack + stk_extra_stack_size);
                          stackoverflow_context_t context = (SIGSEGV_FAULT_CONTEXT);
#else
                          int emergency = 0;
                          stackoverflow_context_t context = (void *) 0;
#endif
                          /* Call user's handler.  */
                          (*stk_user_handler) (emergency, context);
                        }
                    }
                }
            }
        }
#endif /* HAVE_STACK_OVERFLOW_RECOVERY */

      if (user_handler && (*user_handler) (address, 1))
        {
          /* Handler successful.  */
        }
      else
        {
          /* Handler declined responsibility for real.  */

          /* Remove ourselves and dump core.  */
          SIGSEGV_FOR_ALL_SIGNALS (signo, signal (signo, SIG_DFL);)
        }

#if HAVE_STACK_OVERFLOW_RECOVERY
    }
#endif /* HAVE_STACK_OVERFLOW_RECOVERY */
}

#elif HAVE_STACK_OVERFLOW_RECOVERY

static void
#ifdef SIGSEGV_FAULT_STACKPOINTER
sigsegv_handler (SIGSEGV_FAULT_HANDLER_ARGLIST)
#else
sigsegv_handler (int sig)
#endif
{
#if !((HAVE_GETRLIMIT && defined RLIMIT_STACK) || defined SIGSEGV_FAULT_STACKPOINTER)
#error "Insufficient heuristics for detecting a stack overflow.  Either define SIGSEGV_FAULT_STACKPOINTER correctly, or undefine HAVE_STACK_OVERFLOW_RECOVERY!"
#endif

  /* Did the user install a handler?  */
  if (stk_user_handler)
    {
      /* See whether it was a stack overflow.  If so, longjump away.  */
#ifdef SIGSEGV_FAULT_STACKPOINTER
      uintptr_t old_sp = (uintptr_t) (SIGSEGV_FAULT_STACKPOINTER);
#endif

      /* Were we able to determine the stack top?  */
      if (stack_top)
        {
          /* Determine stack bounds.  */
          int saved_errno;
          struct vma_struct vma;
          int ret;

          saved_errno = errno;
          ret = sigsegv_get_vma (stack_top, &vma);
          errno = saved_errno;
          if (ret >= 0)
            {
#if HAVE_GETRLIMIT && defined RLIMIT_STACK
              /* Heuristic BC: If the stack size has reached its maximal size,
                 and old_sp is near the low end, we consider it a stack
                 overflow.  */
              struct rlimit rl;

              saved_errno = errno;
              ret = getrlimit (RLIMIT_STACK, &rl);
              errno = saved_errno;
              if (ret >= 0)
                {
                  uintptr_t current_stack_size = vma.end - vma.start;
                  uintptr_t max_stack_size = rl.rlim_cur;
                  if (current_stack_size <= max_stack_size + 4096
                      && max_stack_size <= current_stack_size + 4096
#else
                {
                  if (1
#endif
#ifdef SIGSEGV_FAULT_STACKPOINTER
                      /* Heuristic BC: If we know old_sp, and it is neither
                         near the low end, nor in the alternate stack, then
                         it's probably not a stack overflow.  */
                      && ((old_sp >= stk_extra_stack
                           && old_sp <= stk_extra_stack + stk_extra_stack_size)
#if STACK_DIRECTION < 0
                          || (old_sp <= vma.start + 4096
                              && vma.start <= old_sp + 4096))
#else
                          || (old_sp <= vma.end + 4096
                              && vma.end <= old_sp + 4096))
#endif
#endif
                     )
                    {
#ifdef SIGSEGV_FAULT_STACKPOINTER
                      int emergency =
                        (old_sp >= stk_extra_stack
                         && old_sp <= stk_extra_stack + stk_extra_stack_size);
                      stackoverflow_context_t context = (SIGSEGV_FAULT_CONTEXT);
#else
                      int emergency = 0;
                      stackoverflow_context_t context = (void *) 0;
#endif
                      /* Call user's handler.  */
                      (*stk_user_handler)(emergency,context);
                    }
                }
            }
        }
    }

  /* Remove ourselves and dump core.  */
  SIGSEGV_FOR_ALL_SIGNALS (signo, signal (signo, SIG_DFL);)
}

#endif


static void
install_for (int sig)
{
  struct sigaction action;

#ifdef SIGSEGV_FAULT_ADDRESS_FROM_SIGINFO
  action.sa_sigaction = (void (*) (int, siginfo_t *, void *)) &sigsegv_handler;
#else
  action.sa_handler = (void (*) (int)) &sigsegv_handler;
#endif
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
#ifdef SIGSEGV_FAULT_ADDRESS_FROM_SIGINFO
  action.sa_flags = SA_SIGINFO;
#else
  action.sa_flags = 0;
#endif
#if HAVE_STACK_OVERFLOW_RECOVERY && HAVE_SIGALTSTACK /* not BeOS */
  /* Work around Linux 2.2.5 bug: If SA_ONSTACK is specified but sigaltstack()
     has not been called, the kernel will busy loop, eating CPU time.  So
     avoid setting SA_ONSTACK until the user has requested stack overflow
     handling.  */
  if (stk_user_handler)
    action.sa_flags |= SA_ONSTACK;
#endif
  sigaction (sig, &action, (struct sigaction *) NULL);
}

int
sigsegv_install_handler (sigsegv_handler_t handler)
{
#if HAVE_SIGSEGV_RECOVERY
  user_handler = handler;

  SIGSEGV_FOR_ALL_SIGNALS (sig, install_for (sig);)

  return 0;
#else
  return -1;
#endif
}

void
sigsegv_deinstall_handler (void)
{
#if HAVE_SIGSEGV_RECOVERY
  user_handler = (sigsegv_handler_t)NULL;

#if HAVE_STACK_OVERFLOW_RECOVERY
  if (!stk_user_handler)
#endif
    {
      SIGSEGV_FOR_ALL_SIGNALS (sig, signal (sig, SIG_DFL);)
    }
#endif
}

int
sigsegv_leave_handler (void (*continuation) (void*, void*, void*),
                       void* cont_arg1, void* cont_arg2, void* cont_arg3)
{
#if HAVE_STACK_OVERFLOW_RECOVERY
  /*
   * Reset the system's knowledge that we are executing on the alternate
   * stack. If we didn't do that, siglongjmp would be needed instead of
   * longjmp to leave the signal handler.
   */
  sigsegv_reset_onstack_flag ();
#endif
  (*continuation) (cont_arg1, cont_arg2, cont_arg3);
  return 1;
}

#if !MIXING_UNIX_SIGSEGV_AND_WIN32_STACKOVERFLOW_HANDLING

int
stackoverflow_install_handler (stackoverflow_handler_t handler,
                               void *extra_stack, size_t extra_stack_size)
{
#if HAVE_STACK_OVERFLOW_RECOVERY
#if HAVE_STACKVMA
  if (!stack_top)
    {
      int dummy;
      remember_stack_top (&dummy);
      if (!stack_top)
        return -1;
    }
#endif

  stk_user_handler = handler;
  stk_extra_stack = (uintptr_t) extra_stack;
  stk_extra_stack_size = extra_stack_size;
#ifdef __BEOS__
  set_signal_stack (extra_stack, extra_stack_size);
#else /* HAVE_SIGALTSTACK */
  {
    stack_t ss;
#if SIGALTSTACK_SS_REVERSED
    ss.ss_sp = (char *) extra_stack + extra_stack_size - sizeof (void *);
    ss.ss_size = extra_stack_size - sizeof (void *);
#else
    ss.ss_sp = extra_stack;
    ss.ss_size = extra_stack_size;
#endif
    ss.ss_flags = 0; /* no SS_DISABLE */
    if (sigaltstack (&ss, (stack_t*)0) < 0)
      return -1;
  }
#endif

  /* Install the signal handlers with SA_ONSTACK.  */
  SIGSEGV_FOR_ALL_SIGNALS (sig, install_for (sig);)
  return 0;
#else
  return -1;
#endif
}

void
stackoverflow_deinstall_handler (void)
{
#if HAVE_STACK_OVERFLOW_RECOVERY
  stk_user_handler = (stackoverflow_handler_t) NULL;

#if HAVE_SIGSEGV_RECOVERY
  if (user_handler)
    {
      /* Reinstall the signal handlers without SA_ONSTACK, to avoid Linux
         bug.  */
      SIGSEGV_FOR_ALL_SIGNALS (sig, install_for (sig);)
    }
  else
#endif
    {
      SIGSEGV_FOR_ALL_SIGNALS (sig, signal (sig, SIG_DFL);)
    }

#ifdef __BEOS__
  /* We cannot undo the effect of set_signal_stack.  */
  fprintf (stderr, "libsigsegv (stackoverflow_deinstall_handler): not supported on this platform\n");
#else /* HAVE_SIGALTSTACK */
  {
    stack_t ss;
    ss.ss_flags = SS_DISABLE;
    if (sigaltstack (&ss, (stack_t *) 0) < 0)
      perror ("libsigsegv (stackoverflow_deinstall_handler)");
  }
#endif

#endif
}

#endif /* !MIXING_UNIX_SIGSEGV_AND_WIN32_STACKOVERFLOW_HANDLING */
