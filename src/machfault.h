/* Fault handler information.
   Copyright (C) 2003  Bruno Haible <bruno@clisp.org>

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
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* The included file defines:

     SIGSEGV_STATE_TYPE
          the machine dependent thread state type.

     SIGSEGV_STATE_FLAVOR
          the corresponding flavor, to be passed to thread_get_state(),
          for retrieving a state of type SIGSEGV_STATE_TYPE.

     SIGSEGV_STATE_COUNT
          the corresponding count (??).

     SIGSEGV_FAULT_ADDRESS
          is a macro for fetching the fault address.
 */

#include CFG_MACHFAULT
