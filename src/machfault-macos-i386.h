/* Fault handler information.  MacOSX/i386 version.
   Copyright (C) 2003-2004  Bruno Haible <bruno@clisp.org>

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

#if 0
#define SIGSEGV_THREAD_STATE_TYPE                   i386_new_thread_state_t
#define SIGSEGV_THREAD_STATE_FLAVOR                 i386_NEW_THREAD_STATE
#define SIGSEGV_THREAD_STATE_COUNT                  i386_NEW_THREAD_STATE_COUNT
#endif
#define SIGSEGV_THREAD_STATE_TYPE                   struct i386_saved_state
#define SIGSEGV_THREAD_STATE_FLAVOR                 i386_SAVED_STATE
#define SIGSEGV_THREAD_STATE_COUNT                  i386_SAVED_STATE_COUNT
/* See comment in /usr/include/mach/i386/thread_status.h.  */
#define SIGSEGV_FAULT_ADDRESS(thr_state,exc_state)  (thr_state).esp
#define SIGSEGV_STACK_POINTER(thr_state)            (thr_state).uesp
#define SIGSEGV_PROGRAM_COUNTER(thr_state)          (thr_state).eip
