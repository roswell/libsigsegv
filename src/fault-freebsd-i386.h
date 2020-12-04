/* Fault handler information.  FreeBSD/i386 version.
   Copyright (C) 2002, 2007, 2020  Bruno Haible <bruno@clisp.org>

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

/* On FreeBSD 12, both of these approaches work.  On FreeBSD derivatives, the
   first one has more chances to work.  */
#if 1
/* Use signal handlers without SA_SIGINFO.  */

#define SIGSEGV_FAULT_HANDLER_ARGLIST  int sig, int code, struct sigcontext *scp, void *addr
#define SIGSEGV_FAULT_ADDRESS  addr
#define SIGSEGV_FAULT_CONTEXT  scp

/* See sys/x86/include/signal.h.  */

#if defined __x86_64__
/* 64 bit registers */

#define SIGSEGV_FAULT_STACKPOINTER  scp->sc_rsp

#else
/* 32 bit registers */

#define SIGSEGV_FAULT_STACKPOINTER  scp->sc_esp

#endif

#else
/* Use signal handlers with SA_SIGINFO.  */

#define SIGSEGV_FAULT_HANDLER_ARGLIST  int sig, siginfo_t *sip, void *scp
#define SIGSEGV_FAULT_ADDRESS  sip->si_addr
#define SIGSEGV_FAULT_CONTEXT  ((struct sigcontext *) scp)
#define SIGSEGV_FAULT_ADDRESS_FROM_SIGINFO

/* See sys/x86/include/signal.h.  */

#if defined __x86_64__
/* 64 bit registers */

#define SIGSEGV_FAULT_STACKPOINTER  ((struct sigcontext *) scp)->sc_rsp

#else
/* 32 bit registers */

#define SIGSEGV_FAULT_STACKPOINTER  ((struct sigcontext *) scp)->sc_esp

#endif

#endif
