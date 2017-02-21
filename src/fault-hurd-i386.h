/* Fault handler information.  Hurd/i386 version.
   Copyright (C) 2017  Bruno Haible <bruno@clisp.org>

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

#include "fault-hurd.h"

/* scp points to a 'struct sigcontext' (defined in
   glibc/sysdeps/mach/hurd/i386/bits/sigcontext.h).
   The registers of this struct get pushed on the stack through
   gnumach/i386/i386/locore.S:trapall.  */
/* Both sc_esp and sc_uesp appear to have the same value.
   It appears more reliable to use sc_uesp because it is labelled as
   "old esp, if trapped from user".  */
#define SIGSEGV_FAULT_STACKPOINTER  scp->sc_uesp
