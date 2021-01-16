/* Fault handler information.  Solaris 11/SPARC version.
   Copyright (C) 2021  Bruno Haible <bruno@clisp.org>

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

#include "fault-solaris-sparc.h"

/* On Solaris 11.3/SPARC, both in 32-bit and 64-bit mode, when catching
   stack overflow, the fault address is correct the first time, but is zero
   or near zero the second time.  'truss tests/stackoverflow1' shows it:

   In 32-bit mode:

    Incurred fault #6, FLTBOUNDS  %pc = 0x000116E8
      siginfo: SIGSEGV SEGV_MAPERR addr=0xFFB00000
    Received signal #11, SIGSEGV [caught]
      siginfo: SIGSEGV SEGV_MAPERR addr=0xFFB00000
   then
    Incurred fault #6, FLTBOUNDS  %pc = 0x000116E8
      siginfo: SIGSEGV SEGV_MAPERR addr=0x00000008
    Received signal #11, SIGSEGV [caught]
      siginfo: SIGSEGV SEGV_MAPERR addr=0x00000008

   In 64-bit mode:

    Incurred fault #6, FLTBOUNDS  %pc = 0x100001C58
      siginfo: SIGSEGV SEGV_MAPERR addr=0xFFFFFFFF7FF00000
    Received signal #11, SIGSEGV [caught]
      siginfo: SIGSEGV SEGV_MAPERR addr=0xFFFFFFFF7FF00000
   then
    Incurred fault #6, FLTBOUNDS  %pc = 0x100001C58
      siginfo: SIGSEGV SEGV_MAPERR addr=0x00000000
    Received signal #11, SIGSEGV [caught]
      siginfo: SIGSEGV SEGV_MAPERR addr=0x00000000
 */
#define BOGUS_FAULT_ADDRESS_UPON_STACK_OVERFLOW
