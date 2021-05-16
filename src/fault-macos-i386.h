/* Fault handler information.  macOS/i386 and macOS/x86_64 version.
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

#include "fault-macos.h"

#if defined __x86_64__
/* 64 bit registers */

/* See the definitions of
     - 'ucontext_t' and 'struct __darwin_ucontext' in <sys/_types/_ucontext.h>,
     - 'struct __darwin_mcontext64' in <i386/_mcontext.h>, and
     - 'struct __darwin_x86_thread_state64' in <mach/i386/_structs.h>.  */
# define SIGSEGV_FAULT_STACKPOINTER  ((ucontext_t *) ucp)->uc_mcontext->__ss.__rsp

#else
/* 32 bit registers */

/* See the definitions of
     - 'ucontext_t' and 'struct __darwin_ucontext' in <sys/_types/_ucontext.h>,
     - 'struct __darwin_mcontext32' in <i386/_mcontext.h>, and
     - 'struct __darwin_i386_thread_state' in <mach/i386/_structs.h>.  */
# define SIGSEGV_FAULT_STACKPOINTER  ((ucontext_t *) ucp)->uc_mcontext->__ss.__esp

#endif
