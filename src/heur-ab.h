/* Detecting stack overflow.  Version for platforms which supply the
   fault address and the stack pointer.
   Copyright (C) 2003  Bruno Haible <bruno@clisp.org>

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

#ifdef __ia64
#define IS_STACK_OVERFLOW \
  (is_stk_overflow ((unsigned long) SIGSEGV_FAULT_ADDRESS,  \
                    (unsigned long) SIGSEGV_FAULT_STACKPOINTER) \
   || is_stk_overflow ((unsigned long) SIGSEGV_FAULT_ADDRESS,  \
                       (unsigned long) SIGSEGV_FAULT_BSP_POINTER))
#else
#define IS_STACK_OVERFLOW \
  is_stk_overflow ((unsigned long) SIGSEGV_FAULT_ADDRESS, \
                   (unsigned long) SIGSEGV_FAULT_STACKPOINTER)
#endif

static int is_stk_overflow (unsigned long addr, unsigned long sp)
{
  return (addr <= sp + 4096 && sp <= addr + 4096);
}

static int
remember_stack_top (void *some_variable_on_stack)
{
  return 0;
}
