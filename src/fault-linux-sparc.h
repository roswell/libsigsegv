/* Fault handler information.  Linux/SPARC version when it supports POSIX.
   Copyright (C) 2009, 2017  Bruno Haible <bruno@clisp.org>

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

#if defined(__sparcv9) || defined(__arch64__) /* 64-bit */
/* From linux-4.8.1/arch/sparc/kernel/signal_64.c, function setup_rt_frame, we
   see that ucp is not an 'ucontext_t *' but rather a 'struct sigcontext *'
   that happens to have the same value as sip (which is possible because a
   'struct sigcontext' starts with 128 bytes room for the siginfo_t).  */
#define SIGSEGV_FAULT_STACKPOINTER  (((struct sigcontext *) ucp)->sigc_regs.u_regs[14] + 2047)
#else /* 32-bit */
/* From linux-4.8.1/arch/sparc/kernel/signal_32.c, function setup_rt_frame,
   and linux-4.8.1/arch/sparc/kernel/signal32.c, function setup_rt_frame32, we
   see that ucp is a 'struct pt_regs *' or 'struct pt_regs32 *', respectively.
   In userland, this is a 'struct sigcontext *'.  */
#define SIGSEGV_FAULT_STACKPOINTER  ((struct sigcontext *) ucp)->si_regs.u_regs[14]
#endif

/* The sip->si_addr field is correct for a normal fault, but unusable in case
   of a stack overflow. What I observe (when running tests/stackoverflow1, with
   a printf right at the beginning of sigsegv_handler) is that sip->si_addr is
   near 0:
     - in 64-bit mode: sip->si_addr = 0x000000000000030F, and gdb shows me that
       the fault occurs in an instruction 'stx %o3,[%fp+0x30f]' and %fp is 0.
       In fact, all registers %l0..%l7 and %i0..%i7 are 0.
     - in 32-bit mode: sip->si_addr = 0xFFFFFA64, and gdb shows me that
       the fault occurs in an instruction 'st %g2,[%fp-1436]' and %fp is 0.
       In fact, all registers %l0..%l7 and %i0..%i7 are 0.
   Apparently when the stack overflow occurred, some trap has tried to move the
   contents of the registers %l0..%l7 and %i0..%i7 (a "window" in SPARC
   terminology) to the stack, did not succeed in doing this, replaced all these
   register values with 0, and resumed execution at the fault location. This
   time, due to %fp = 0, a different fault was triggered. Now it is impossible
   to determine the real (previous) fault address because, even if know the
   faulting instruction, the previous register values have been lost.  */
#define BOGUS_FAULT_ADDRESS_UPON_STACK_OVERFLOW
