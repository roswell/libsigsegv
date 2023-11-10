/* Fault handler information.  FreeBSD/ARM version when it supports POSIX.
   Copyright (C) 2020-2023  Bruno Haible <bruno@clisp.org>

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

#define SIGSEGV_FAULT_HANDLER_ARGLIST  int sig, siginfo_t *sip, void *ucp
#define SIGSEGV_FAULT_ADDRESS  sip->si_addr
#define SIGSEGV_FAULT_CONTEXT  ((ucontext_t *) ucp)

#if defined(__aarch64__) || defined(__ARM_64BIT_STATE) || defined(__ARM_PCS_AAPCS64) /* 64-bit */

/* See sys/arm64/include/ucontext.h.  */

#if defined(__CHERI__)
#define SIGSEGV_FAULT_STACKPOINTER  ((ucontext_t *) ucp)->uc_mcontext.mc_capregs.cap_sp
#else
#define SIGSEGV_FAULT_STACKPOINTER  ((ucontext_t *) ucp)->uc_mcontext.mc_gpregs.gp_sp
#endif

#else /* 32-bit */

/* See sys/arm/include/ucontext.h.  */

#define SIGSEGV_FAULT_STACKPOINTER  ((ucontext_t *) ucp)->uc_mcontext.__gregs[_REG_SP]

#endif
