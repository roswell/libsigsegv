/* Test that libsigsegv does not interfere with fault handling inside
   system calls.
   Copyright (C) 2009-2010  Eric Blake <ebb9@byu.net>

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
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

#ifndef _MSC_VER
# include <config.h>
#endif

#include "sigsegv.h"

#if HAVE_STACK_OVERFLOW_RECOVERY && HAVE_EFAULT_SUPPORT

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "altstack.h"

/* A NULL pointer.
   If we were to use a literal NULL, gcc would give a warning on glibc systems:
   "warning: null argument where non-null required".  */
const char *null_pointer = NULL;

static void
handler (int emergency, stackoverflow_context_t scp)
{
  /* This test is only enabled when ENABLE_EFAULT is true.  If we get here,
     the ENABLE_EFAULT handling in libsigsegv is incomplete.  */
  abort ();
}

int
main ()
{
  /* Test whether the OS handles some faults by itself.  */
  if (open (null_pointer, O_RDONLY) != -1 || errno != EFAULT)
    {
      fprintf (stderr, "EFAULT not detected alone.\n");
      exit (1);
    }

  /* Prepare the storage for the alternate stack.  */
  prepare_alternate_stack ();

  /* Install the stack overflow handler.  */
  if (stackoverflow_install_handler (&handler, mystack, SIGSTKSZ) < 0)
    exit (2);

  /* Test that the library does not interfere with OS faults.  */
  if (open (null_pointer, O_RDONLY) != -1 || errno != EFAULT)
    {
      fprintf (stderr, "EFAULT not detected with handler.\n");
      exit (1);
    }

  /* Validate that the alternate stack did not overflow.  */
  check_alternate_stack_no_overflow ();

  /* Test passed!  */
  printf ("Test passed.\n");
  return 0;
}

#else

int
main ()
{
  return 77;
}

#endif
