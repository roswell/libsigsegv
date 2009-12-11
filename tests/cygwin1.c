/* Test that libsigsegv does not interfere with internal fault handling
   inside system calls.
   Copyright (C) 2009  Eric Blake <ebb9@byu.net>

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

#if HAVE_SIGSEGV_RECOVERY

/* Cygwin uses faults for internal purposes, i.e. even when the user passes
   valid pointers to all system calls.

   An example is pthread_attr_init.  POSIX:2001 states pthread_attr_init may
   fail with EBUSY if an object was previously initialized.  Recognizing
   whether an object was previously initialized can be done with a magic
   cookie.  If the type pthread_attr_t is defined as
     struct { int magic; ... other_data; }
   it is easy.  But in Cygwin, pthread_attr_t is defined as a pointer type.
   Cygwin implements the check by testing whether the pthread_attr_t contains
   a pointer to a memory region that contains a particular magic cookie.
   This indirection may fault.  (Search for pthread_attr_init and
   verifyable_object_isvalid in the Cygwin sources.)  In this example we trick
   Cygwin into dereferencing the address 0x01010101.  */

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

static int
handler (void *fault_address, int serious)
{
  exit (2);
}

int
main ()
{
  pthread_attr_t a;

  sigsegv_install_handler (handler);

  /* Set up a pthread_attr_t that contains a pointer to address 0x01010101.  */
  memset (&a, 1, sizeof (a));

  /* Make a system call to initialize it.  It should succeed and not invoke
     the handler.  */
  pthread_attr_init (&a);

  return 0;
}

#endif
