/* Support functions for Windows on x86_64 CPU
   Copyright (C) 2026  Bruno Haible <bruno@clisp.org>

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

#include "sigsegv.h"

void
stackoverflow_longjmp (jmp_buf env, int val)
{
  /* On 64-bit native Windows, the Microsoft library has setjmp/longjmp
     implementations that unwind the stack, in order to execute C++ destructors.
     <https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/setjmp>
     <https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/longjmp>
     Here, when jumping back from the alternate stack to the main stack, this
     unwinding mechanism gets confused and triggers an exception with code
     0xC0000028 = STATUS_BAD_STACK.
     To avoid this, overwrite the first word of the jmp_buf (called the Frame
     word) with NULL.  Then, on x86_64, longjmp() skips the unwinding.  See
     <https://blog.lazym.io/2020/09/21/Unicorn-Devblog-setjmp-longjmp-on-Windows/>
     On mingw 14 and older, this is needed too.  Only on mingw (~2026) this is
     not a problem any more, because these versions of mingw have their own
     setjmp() that stores NULL in the Frame word and their own longjmp() that
     always skips the unwinding.  */
  *(void **)env = NULL;

  longjmp (env, val);
}
