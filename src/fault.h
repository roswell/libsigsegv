/* Fault handler information.
   Copyright (C) 2002, 2021  Bruno Haible <bruno@clisp.org>

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

/* The included file defines:

     SIGSEGV_FAULT_HANDLER_ARGLIST
          is the argument list for the actual fault handler.

   and if available (optional):

     SIGSEGV_FAULT_ADDRESS
          is a macro for fetching the fault address.

     SIGSEGV_FAULT_CONTEXT
          is a macro giving a pointer to the entire fault context (i.e.
          the register set etc.).

     SIGSEGV_FAULT_STACKPOINTER
          is a macro for fetching the stackpointer at the moment the fault
          occurred.
 */

#include CFG_FAULT
