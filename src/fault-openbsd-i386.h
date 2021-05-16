/* Fault handler information.  OpenBSD/i386 and OpenBSD/x86_64 version.
   Copyright (C) 2003, 2010  Bruno Haible <bruno@clisp.org>

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

#include "fault-openbsd.h"

#if defined __x86_64__
/* 64 bit registers */

/* See the definition of 'struct sigcontext' in
   openbsd-src/sys/arch/amd64/include/signal.h.  */

# define SIGSEGV_FAULT_STACKPOINTER  scp->sc_rsp

#else
/* 32 bit registers */

/* See the definition of 'struct sigcontext' in
   openbsd-src/sys/arch/i386/include/signal.h.  */

# define SIGSEGV_FAULT_STACKPOINTER  scp->sc_esp

#endif
