/* Determine the virtual memory area of a given address.  AIX version.
   Copyright (C) 2021  Bruno Haible <bruno@clisp.org>

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

#include "stackvma.h"

#include "stackvma-mincore.c"

int
sigsegv_get_vma (uintptr_t address, struct vma_struct *vma)
{
  return mincore_get_vma (address, vma);
}
