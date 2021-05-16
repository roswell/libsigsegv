/* Fault handler information.  Linux/HPPA version.
   Copyright (C) 2002-2003, 2009  Bruno Haible <bruno@clisp.org>

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

#include <ucontext.h>

#define SIGSEGV_FAULT_HANDLER_ARGLIST  int sig, siginfo_t *sip, void *ucp
#define SIGSEGV_FAULT_ADDRESS  sip->si_ptr
#define SIGSEGV_FAULT_ADDRESS_FROM_SIGINFO

#if 0
#define SIGSEGV_FAULT_CONTEXT  ((ucontext_t *) ucp)
#define SIGSEGV_FAULT_STACKPOINTER  ((ucontext_t *) ucp)->uc_mcontext.gregs.g_regs[30]
#endif
#if 0
#define SIGSEGV_FAULT_CONTEXT  ((ucontext_t *) ucp)
#define SIGSEGV_FAULT_STACKPOINTER  ((ucontext_t *) ucp)->uc_mcontext.sc_gr[30]
#endif
