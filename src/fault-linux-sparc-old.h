/* Fault handler information.  Linux/SPARC version.
   Copyright (C) 2002, 2009, 2017  Bruno Haible <bruno@clisp.org>

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

#include <asm/sigcontext.h>

#define SIGSEGV_FAULT_HANDLER_ARGLIST  int sig, int code, struct sigcontext *scp, void *addr /* FIXME */
#define SIGSEGV_FAULT_ADDRESS  addr  /* in case of SunOS4 signal frames */
#define SIGSEGV_FAULT_CONTEXT  scp
#if 1 /* Old? FIXME */
# define SIGSEGV_FAULT_STACKPOINTER  scp->sigc_sp
#else
# if defined(__sparcv9) || defined(__arch64__) /* 64-bit */
#  define SIGSEGV_FAULT_STACKPOINTER  scp->sigc_regs.u_regs[14]
# else /* 32-bit */
#  define SIGSEGV_FAULT_STACKPOINTER  scp->si_regs.u_regs[14]
# endif
#endif
