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
   The registers, at the moment the signal occurred, get pushed on the kernel
   stack through gnumach/x86_64/locore.S:alltraps. They are denoted by a
   'struct i386_saved_state' (defined in gnumach/i386/i386/thread.h).
   Upon invocation of the Mach interface function thread_get_state
   <https://www.gnu.org/software/hurd/gnumach-doc/Thread-Execution.html>
   (= __thread_get_state in glibc), defined in gnumach/kern/thread.c,
   the function thread_getstatus, defined in gnumach/i386/i386/pcb.c, copies the
   register values in a different arrangement into a 'struct i386_thread_state',
   defined in gnumach/i386/include/mach/i386/thread_status.h. (Different
   arrangement: trapno, err get dropped; different order of r8...r15; also rsp
   gets set to 0.)
   This 'struct i386_thread_state' is actually the 'basic' part of a
   'struct machine_thread_all_state', defined in
   glibc/sysdeps/mach/x86/thread_state.h.
   From there, the function _hurd_setup_sighandler, defined in
   glibc/sysdeps/mach/hurd/x86/trampoline.c,
   1. sets rsp to the same value as ursp,
   2. copies the 'struct i386_thread_state' into the appropriate part of a
      'struct sigcontext', defined in
      glibc/sysdeps/mach/hurd/x86_64/bits/sigcontext.h.  */
/* Both sc_rsp and sc_ursp have the same value.
   It appears more reliable to use sc_ursp because sc_rsp is marked as
   "not used".  */
# define SIGSEGV_FAULT_STACKPOINTER  scp->sc_ursp

#else
/* 32 bit registers */

/* scp points to a 'struct sigcontext' (defined in
   glibc/sysdeps/mach/hurd/i386/bits/sigcontext.h).
   The registers, at the moment the signal occurred, get pushed on the kernel
   stack through gnumach/i386/i386/locore.S:alltraps. They are denoted by a
   'struct i386_saved_state' (defined in gnumach/i386/i386/thread.h).
   Upon invocation of the Mach interface function thread_get_state
   <https://www.gnu.org/software/hurd/gnumach-doc/Thread-Execution.html>
   (= __thread_get_state in glibc), defined in gnumach/kern/thread.c,
   the function thread_getstatus, defined in gnumach/i386/i386/pcb.c, copies the
   register values in a different arrangement into a 'struct i386_thread_state',
   defined in gnumach/i386/include/mach/i386/thread_status.h. (Different
   arrangement: trapno, err get dropped; also esp gets set to 0.)
   This 'struct i386_thread_state' is actually the 'basic' part of a
   'struct machine_thread_all_state', defined in
   glibc/sysdeps/mach/x86/thread_state.h.
   From there, the function _hurd_setup_sighandler, defined in
   glibc/sysdeps/mach/hurd/x86/trampoline.c,
   1. sets esp to the same value as uesp,
   2. copies the 'struct i386_thread_state' into the appropriate part of a
      'struct sigcontext', defined in
      glibc/sysdeps/mach/hurd/i386/bits/sigcontext.h.  */
/* Both sc_esp and sc_uesp have the same value.
   It appears more reliable to use sc_uesp because sc_esp is marked as
   "not used".  */
# define SIGSEGV_FAULT_STACKPOINTER  scp->sc_uesp

#endif
