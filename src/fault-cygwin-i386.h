/* Fault handler information.  Cygwin/i386 and Cygwin/x86_64 version.
   Copyright (C) 2021  Bruno Haible <bruno@clisp.org>

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

#include "fault-cygwin.h"

/* See the definition of 'ucontext_t' in <sys/ucontext.h> and
   of 'struct __mcontext' in <cygwin/signal.h>.  */
#if defined __x86_64__
/* 64 bit registers */
# define SIGSEGV_FAULT_STACKPOINTER  ((ucontext_t *) ucp)->uc_mcontext.rsp
#elif defined __i386__
/* 32 bit registers */
# define SIGSEGV_FAULT_STACKPOINTER  ((ucontext_t *) ucp)->uc_mcontext.esp
#endif
