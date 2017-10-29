/* Fault handler information.  Haiku/i386 version.
   Copyright (C) 2017  Bruno Haible <bruno@clisp.org>

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

#include "fault-haiku.h"

#if defined __x86_64__
/* 64 bit registers */

/* See the definition of 'ucontext_t' in <signal.h> and
   of 'struct vregs' in <arch/x86_64/signal.h>.  */

# define SIGSEGV_FAULT_STACKPOINTER  ((ucontext_t *) ucp)->uc_mcontext.rsp

#else
/* 32 bit registers */

/* See the definition of 'ucontext_t' in <signal.h> and
   of 'struct vregs' in <arch/x86/signal.h>.  */

# define SIGSEGV_FAULT_STACKPOINTER  ((ucontext_t *) ucp)->uc_mcontext.esp

#endif
