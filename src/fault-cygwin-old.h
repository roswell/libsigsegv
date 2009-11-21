/* Fault handler information.  Cygwin 1.5.x version.
   Copyright (C) 2009  Eric Blake <ebb9@byu.net>

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

/* Cygwin 1.5.x sets siginfo_t.si_addr to the instruction that faulted, not
   the address that the instruction tried to access.  This is fixed in
   Cygwin 1.7.  As a workaround, we use the fault address that was included
   in the EXCEPTION_POINTERS structure when Win32 signalled the exception.
   main_exception_filter stores this fault address before it passes control to
   Cygwin's exception filter which ultimately invokes the Unix signal
   handler.  */

#define SIGSEGV_FAULT_HANDLER_ARGLIST  int sig, siginfo_t *sip, void *context
#define SIGSEGV_FAULT_ADDRESS  last_seen_fault_address
