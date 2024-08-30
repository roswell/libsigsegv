# sigaltstack-longjmp.m4
# serial 8 (libsigsegv-2.15)
dnl Copyright (C) 2002-2024 Bruno Haible <bruno@clisp.org>
dnl This file is free software, distributed under the terms of the GNU
dnl General Public License as published by the Free Software Foundation;
dnl either version 2 of the License, or (at your option) any later version.
dnl As a special exception to the GNU General Public License, this file
dnl may be distributed as part of a program that contains a configuration
dnl script generated by Autoconf, under the same distribution terms as
dnl the rest of that program.

dnl How to longjmp out of a signal handler, in such a way that the
dnl alternate signal stack remains functional.
dnl SV_TRY_LEAVE_HANDLER_LONGJMP(KIND, CACHESYMBOL, KNOWN-SYSTEMS,
dnl                              INCLUDES, RESETCODE)
AC_DEFUN([SV_TRY_LEAVE_HANDLER_LONGJMP],
[
  AC_REQUIRE([AC_PROG_CC])
  AC_REQUIRE([AC_CANONICAL_HOST])

  AC_CACHE_CHECK([whether a signal handler can be left through longjmp$1], [$2], [
    AC_RUN_IFELSE([
      AC_LANG_SOURCE([[
#include <stdlib.h>
#include <signal.h>
#include <setjmp.h>
$4
#if HAVE_SETRLIMIT
# include <sys/types.h>
# include <sys/time.h>
# include <sys/resource.h>
#endif
/* In glibc >= 2.34, when _GNU_SOURCE is defined, SIGSTKSZ is no longer a
   compile-time constant.  But we need a simple constant here.  */
#if __GLIBC__ >= 2
# undef SIGSTKSZ
# if defined __ia64__
#  define SIGSTKSZ 262144
# else
#  define SIGSTKSZ 16384
# endif
#endif
#ifndef SIGSTKSZ
# define SIGSTKSZ 16384
#endif
jmp_buf mainloop;
sigset_t mainsigset;
int pass = 0;
void stackoverflow_handler (int sig)
{
  pass++;
  sigprocmask (SIG_SETMASK, &mainsigset, NULL);
  { $5 }
  longjmp (mainloop, pass);
}
volatile int * recurse_1 (volatile int n, volatile int *p)
{
  if (n >= 0)
    *recurse_1 (n + 1, p) += n;
  return p;
}
int recurse (volatile int n)
{
  int sum = 0;
  return *recurse_1 (n, &sum);
}
char mystack[2 * SIGSTKSZ];
int main ()
{
  stack_t altstack;
  struct sigaction action;
  sigset_t emptyset;
#if defined HAVE_SETRLIMIT && defined RLIMIT_STACK
  /* Before starting the endless recursion, try to be friendly to the user's
     machine.  On some Linux 2.2.x systems, there is no stack limit for user
     processes at all.  We don't want to kill such systems.  */
  struct rlimit rl;
  rl.rlim_cur = rl.rlim_max = 0x100000; /* 1 MB */
  setrlimit (RLIMIT_STACK, &rl);
#endif
  /* Install the alternate stack.  Use the midpoint of mystack, to guard
     against a buggy interpretation of ss_sp on IRIX.  */
  altstack.ss_sp = mystack + SIGSTKSZ;
  altstack.ss_size = SIGSTKSZ;
  altstack.ss_flags = 0; /* no SS_DISABLE */
  if (sigaltstack (&altstack, NULL) < 0)
    exit (1);
  /* Install the SIGSEGV handler.  */
  sigemptyset (&action.sa_mask);
  action.sa_handler = &stackoverflow_handler;
  action.sa_flags = SA_ONSTACK;
  sigaction (SIGSEGV, &action, (struct sigaction *) NULL);
  sigaction (SIGBUS, &action, (struct sigaction *) NULL);
  /* Save the current signal mask.  */
  sigemptyset (&emptyset);
  sigprocmask (SIG_BLOCK, &emptyset, &mainsigset);
  /* Provoke two stack overflows in a row.  */
  if (setjmp (mainloop) < 2)
    {
      recurse (0);
      exit (2);
    }
  exit (0);
}]])],
      [$2=yes],
      [$2=no],
      [case "$host" in
         m4_if([$3], [], [], [[$3]) $2=yes ;;])
         *) $2="guessing no" ;;
       esac
      ])
  ])
])
