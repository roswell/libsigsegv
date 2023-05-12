/* Fault handler information.  Hurd/i386 and Hurd/x86_64 versions.
   Copyright (C) 2017, 2023  Bruno Haible <bruno@clisp.org>

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

#include "fault-hurd.h"

#if defined __x86_64__
/* 64 bit registers */

/* scp points to a 'struct sigcontext' (defined in
   glibc/sysdeps/mach/hurd/x86_64/bits/sigcontext.h).
   The registers, at the moment the signal occurred, get pushed on the stack
   through gnumach/x86_64/locore.S:alltraps and then copied into the struct
   through glibc/sysdeps/mach/hurd/x86/trampoline.c.  */
/* sc_rsp is unused (not set by gnumach/x86_64/locore.S:alltraps).  We need
   to use sc_ursp.  */
# define SIGSEGV_FAULT_STACKPOINTER  scp->sc_ursp

#else
/* 32 bit registers */

/* scp points to a 'struct sigcontext' (defined in
   glibc/sysdeps/mach/hurd/i386/bits/sigcontext.h).
   The registers, at the moment the signal occurred, get pushed on the stack
   through gnumach/i386/i386/locore.S:alltraps and then copied into the struct
   through glibc/sysdeps/mach/hurd/x86/trampoline.c.  */
/* Both sc_esp and sc_uesp appear to have the same value.
   It appears more reliable to use sc_uesp because it is labelled as
   "old esp, if trapped from user".  */
# define SIGSEGV_FAULT_STACKPOINTER  scp->sc_uesp

#endif
