/* Fault handler information.  Linux/HPPA version.
   Copyright (C) 2002  Bruno Haible <bruno@clisp.org>

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
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include <siginfo.h>
#include <ucontext.h>

#define SIGSEGV_FAULT_HANDLER_ARGLIST  int sig, siginfo_t *sip, void *ucp
#define SIGSEGV_FAULT_ADDRESS  sip->si_addr
#define SIGSEGV_FAULT_CONTEXT  ((ucontext_t *) ucp)
#define SIGSEGV_FAULT_STACKPOINTER  ((ucontext_t *) ucp)->uc_mcontext.gregs.g_regs[30]

#if 0

#include <asm/sigcontext.h>

#define SIGSEGV_FAULT_HANDLER_ARGLIST  int sig, ... scp
#define SIGSEGV_FAULT_CONTEXT  scp
#define SIGSEGV_FAULT_STACKPOINTER  scp->sc_gr[30]

#endif
