# fault.m4
# serial 11 (libsigsegv-2.15)
dnl Copyright (C) 2002-2024 Bruno Haible <bruno@clisp.org>
dnl This file is free software, distributed under the terms of the GNU
dnl General Public License.  As a special exception to the GNU General
dnl Public License, this file may be distributed as part of a program
dnl that contains a configuration script generated by Autoconf, under
dnl the same distribution terms as the rest of that program.

dnl How to write a SIGSEGV handler with access to the fault address.
dnl SV_TRY_FAULT(KIND, CACHESYMBOL, KNOWN-SYSTEMS,
dnl              INCLUDES, FAULT_HANDLER_ARGLIST, FAULT_ADDRESS, [INSTALLCODE])
AC_DEFUN([SV_TRY_FAULT], [
  AC_REQUIRE([AC_PROG_CC])
  AC_REQUIRE([AC_CANONICAL_HOST])

  AC_CACHE_CHECK([whether a fault handler according to $1 works], [$2], [
    AC_RUN_IFELSE([
      AC_LANG_SOURCE([[
#include <stdint.h>
#include <stdlib.h>
#include <signal.h>
#if HAVE_SYS_SIGNAL_H
# include <sys/signal.h>
#endif
$4
#include <sys/types.h>
#include <sys/mman.h>
#if HAVE_MMAP_DEVZERO
# include <fcntl.h>
# ifndef MAP_FILE
#  define MAP_FILE 0
# endif
#endif
#ifndef PROT_NONE
# define PROT_NONE 0
#endif
#if HAVE_MMAP_ANON
# define zero_fd -1
# define map_flags MAP_ANON | MAP_PRIVATE
#elif HAVE_MMAP_ANONYMOUS
# define zero_fd -1
# define map_flags MAP_ANONYMOUS | MAP_PRIVATE
#elif HAVE_MMAP_DEVZERO
static int zero_fd;
# define map_flags MAP_FILE | MAP_PRIVATE
#endif
#if defined __NetBSD__ && (defined __sparc__ || defined __sparc64__)
  /* getpagesize () is 0x1000 or 0x2000, depending on hardware.  */
# include <unistd.h>
# define SIGSEGV_FAULT_ADDRESS_ROUNDOFF_BITS (getpagesize () - 1)
#elif defined __linux__ && (defined __s390__ || defined __s390x__)
# define SIGSEGV_FAULT_ADDRESS_ROUNDOFF_BITS (0x1000UL - 1)
#else
# define SIGSEGV_FAULT_ADDRESS_ROUNDOFF_BITS 0UL
#endif
uintptr_t volatile page;
int volatile handler_called = 0;
void sigsegv_handler ($5)
{
  void *fault_address = (void *) ($6);
  handler_called++;
  if (handler_called == 10)
    exit (4);
  if (fault_address
      != (void*)((page + 0x678) & ~SIGSEGV_FAULT_ADDRESS_ROUNDOFF_BITS))
    exit (3);
  if (mprotect ((void *) page, 0x10000, PROT_READ | PROT_WRITE) < 0)
    exit (2);
}
void crasher (uintptr_t p)
{
  *(int *) (p + 0x678) = 42;
}
int main ()
{
  void *p;
  struct sigaction action;
  /* Preparations.  */
#if !HAVE_MMAP_ANON && !HAVE_MMAP_ANONYMOUS && HAVE_MMAP_DEVZERO
  zero_fd = open ("/dev/zero", O_RDONLY, 0644);
#endif
  /* Setup some mmaped memory.  */
#ifdef __hpux
  /* HP-UX 10 mmap() often fails when given a hint.  So give the OS complete
     freedom about the address range.  */
  p = mmap ((void *) 0,          0x10000, PROT_READ | PROT_WRITE, map_flags, zero_fd, 0);
#else
  p = mmap ((void *) 0x12340000, 0x10000, PROT_READ | PROT_WRITE, map_flags, zero_fd, 0);
#endif
  if (p == (void *)(-1))
    exit (2);
  page = (uintptr_t) p;
  /* Make it read-only.  */
#if defined __linux__ && defined __sparc__
  /* On Linux 2.6.26/SPARC64, PROT_READ has the same effect as
     PROT_READ | PROT_WRITE.  */
  if (mprotect ((void *) page, 0x10000, PROT_NONE) < 0)
#else
  if (mprotect ((void *) page, 0x10000, PROT_READ) < 0)
#endif
    exit (2);
  /* Install the SIGSEGV handler.  */
  sigemptyset(&action.sa_mask);
]m4_if([$7], [], [
  action.sa_handler = (void (*) (int)) &sigsegv_handler;
  action.sa_flags = 0;
], [$7])[
  sigaction (SIGSEGV, &action, (struct sigaction *) NULL);
  sigaction (SIGBUS, &action, (struct sigaction *) NULL);
  /* The first write access should invoke the handler and then complete.  */
  crasher (page);
  /* The second write access should not invoke the handler.  */
  crasher (page);
  /* Check that the handler was called only once.  */
  if (handler_called != 1)
    exit (1);
  /* Test passed!  */
  return 0;
}]])],
      [$2=yes],
      [$2=no],
      [case "$host" in
         m4_if([$3], [], [], [[$3]) $2=yes ;;])
         *)
           AC_LINK_IFELSE([
             AC_LANG_PROGRAM([[
#include <signal.h>
$4
void sigsegv_handler ($5)
{
  void *fault_address = (void *) ($6);
}
]],
               [[struct sigaction action;
$7]])],
             [$2="guessing no"],
             [$2=no])
           ;;
       esac
      ])
  ])
])
