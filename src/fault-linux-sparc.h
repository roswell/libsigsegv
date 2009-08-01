/* Fault handler information.  Linux/SPARC version when it supports POSIX.
   Copyright (C) 2009  Bruno Haible <bruno@clisp.org>

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

#include "fault-posix-ucontext.h"

/* See glibc/sysdeps/unix/sysv/linux/sparc/sys/ucontext.h
   and the definition of GET_STACK in
   glibc/sysdeps/unix/sysv/linux/sparc/{sparc32,sparc64}/sigcontextinfo.h.
   Note that the 'mcontext_t' defined in
   glibc/sysdeps/unix/sysv/linux/sparc/sys/ucontext.h
   and the 'struct sigcontext' defined in
   glibc/sysdeps/unix/sysv/linux/sparc/bits/sigcontext.h
   (see also <asm/sigcontext.h>)
   are quite different types.  */

#define SIGSEGV_FAULT_STACKPOINTER  ((ucontext_t *) ucp)->uc_mcontext.gregs[REG_O6]
