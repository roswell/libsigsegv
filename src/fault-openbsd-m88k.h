/* Fault handler information.  OpenBSD/m88k version.
   Copyright (C) 2010  Bruno Haible <bruno@clisp.org>

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

#include <sys/param.h> /* defines macro OpenBSD */

/* See the definition of 'struct sigcontext' in
   openbsd-src/sys/arch/m88k/include/signal.h
   and the definition of 'struct reg' in
   openbsd-src/sys/arch/m88k/include/reg.h.  */

#if OpenBSD >= 201211 /* OpenBSD version >= 5.2 */
# define SIGSEGV_FAULT_STACKPOINTER  scp->sc_regs[31]
#else
# define SIGSEGV_FAULT_STACKPOINTER  scp->sc_regs.r[31]
#endif
