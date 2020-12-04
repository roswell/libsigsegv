/* Fault handler information.  FreeBSD/ARM version when it supports POSIX.
   Copyright (C) 2020  Bruno Haible <bruno@clisp.org>

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

#define SIGSEGV_FAULT_HANDLER_ARGLIST  int sig, siginfo_t *sip, void *ucp
#define SIGSEGV_FAULT_ADDRESS  sip->si_addr
#define SIGSEGV_FAULT_CONTEXT  ((ucontext_t *) ucp)

#if defined(__aarch64__) || defined(__ARM_64BIT_STATE) || defined(__ARM_PCS_AAPCS64) /* 64-bit */

/* See sys/arm64/include/ucontext.h.  */

#define SIGSEGV_FAULT_STACKPOINTER  ((ucontext_t *) ucp)->uc_mcontext.mc_gpregs.gp_sp

#else /* 32-bit */

/* See sys/arm/include/ucontext.h.  */

#define SIGSEGV_FAULT_STACKPOINTER  ((ucontext_t *) ucp)->uc_mcontext.__gregs[_REG_SP]

#endif
