/* Fault handler information.  NetBSD/i386 version.
   Copyright (C) 2006  Bruno Haible <bruno@clisp.org>

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

#include "fault-netbsd.h"

#ifdef _UC_MACHINE_SP /* introduced in NetBSD 2.0 */

/* _UC_MACHINE_SP is the same as    ->uc_mcontext.__gregs[_REG_UESP],
   and apparently the same value as ->uc_mcontext.__gregs[_REG_ESP]. */
#define SIGSEGV_FAULT_STACKPOINTER  _UC_MACHINE_SP ((ucontext_t *) ucp)

#endif
