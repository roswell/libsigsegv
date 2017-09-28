/* Determine the virtual memory area of a given address.  Linux version.
   Copyright (C) 2002, 2006, 2008, 2016-2017  Bruno Haible <bruno@clisp.org>

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

#include "stackvma.h"
#include <stdio.h>

#include "stackvma-simple.c"
#include "stackvma-rofile.c"

#if HAVE_MINCORE
# define sigsegv_get_vma mincore_get_vma
# define STATIC static
# include "stackvma-mincore.c"
# undef sigsegv_get_vma
#endif

int
sigsegv_get_vma (uintptr_t address, struct vma_struct *vma)
{
  struct rofile rof;
  uintptr_t auxmap_start;
  uintptr_t auxmap_end;
  int c;
  uintptr_t start, end;
#if STACK_DIRECTION < 0
  uintptr_t prev;
#endif

  /* Open the current process' maps file.  It describes one VMA per line.  */
  if (rof_open (&rof, "/proc/self/maps") < 0)
    goto failed;

  auxmap_start = rof.auxmap_start;
  auxmap_end = rof.auxmap_end;
#if STACK_DIRECTION < 0
  prev = 0;
#endif
  for (;;)
    {
      if (!(rof_scanf_lx (&rof, &start) >= 0
            && rof_getchar (&rof) == '-'
            && rof_scanf_lx (&rof, &end) >= 0))
        break;
      while (c = rof_getchar (&rof), c != -1 && c != '\n')
        continue;
      if (start <= auxmap_start && auxmap_end - 1 <= end - 1)
        {
          /* Consider [start,end-1] \ [auxmap_start,auxmap_end-1]
             = [start,auxmap_start-1] u [auxmap_end,end-1].  */
          if (start != auxmap_start)
            {
              if (address >= start && address <= auxmap_start - 1)
                {
                  if (end != auxmap_end)
                    {
                      vma->start = start;
                      vma->end = end;
#if STACK_DIRECTION < 0
                      vma->prev_end = prev;
#else
                      vma->next_start = auxmap_end;
#endif
                      goto found2;
                    }
                  else
                    {
                      end = auxmap_start;
                      goto found;
                    }
                }
#if STACK_DIRECTION < 0
              prev = auxmap_start;
#endif
            }
          if (end != auxmap_end)
            {
              if (address >= auxmap_end && address <= end - 1)
                {
                  start = auxmap_end;
                  goto found;
                }
#if STACK_DIRECTION < 0
              prev = end;
#endif
            }
        }
      else
        {
          if (address >= start && address <= end - 1)
            goto found;
#if STACK_DIRECTION < 0
          prev = end;
#endif
        }
    }
  rof_close (&rof);
 failed:
#if HAVE_MINCORE
  return mincore_get_vma (address, vma);
#else
  return -1;
#endif

 found:
  vma->start = start;
  vma->end = end;
#if STACK_DIRECTION < 0
  vma->prev_end = prev;
#else
  if (!(rof_scanf_lx (&rof, &vma->next_start) >= 0
        && rof_getchar (&rof) == '-'
        && rof_scanf_lx (&rof, &end) >= 0))
    vma->next_start = 0;
#endif
 found2:
  rof_close (&rof);
  vma->is_near_this = simple_is_near_this;
  return 0;
}
