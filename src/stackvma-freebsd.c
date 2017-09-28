/* Determine the virtual memory area of a given address.  FreeBSD version.
   Copyright (C) 2002-2003, 2006, 2008, 2011, 2016-2017  Bruno Haible <bruno@clisp.org>

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
  /* The stack appears as multiple adjacents segments, therefore we
     merge adjacent segments.  */
  uintptr_t curr_start, curr_end;
#if STACK_DIRECTION < 0
  uintptr_t prev_end;
#endif

  /* Open the current process' maps file.  It describes one VMA per line.
     Cf. <http://www.freebsd.org/cgi/cvsweb.cgi/src/sys/fs/procfs/procfs_map.c?annotate=HEAD> */
  if (rof_open (&rof, "/proc/curproc/map") < 0)
    goto failed;

  auxmap_start = rof.auxmap_start;
  auxmap_end = rof.auxmap_end;
#if STACK_DIRECTION < 0
  prev_end = 0;
#endif
  for (curr_start = curr_end = 0; ;)
    {
      uintptr_t next_start, next_end;
      if (!(rof_getchar (&rof) == '0'
            && rof_getchar (&rof) == 'x'
            && rof_scanf_lx (&rof, &next_start) >= 0))
        break;
      while (c = rof_peekchar (&rof), c == ' ' || c == '\t')
        rof_getchar (&rof);
      if (!(rof_getchar (&rof) == '0'
            && rof_getchar (&rof) == 'x'
            && rof_scanf_lx (&rof, &next_end) >= 0))
        break;
      while (c = rof_getchar (&rof), c != -1 && c != '\n')
        continue;
      if (next_start <= auxmap_start && auxmap_end - 1 <= next_end - 1)
        {
          /* Consider [next_start,next_end-1] \ [auxmap_start,auxmap_end-1]
             = [next_start,auxmap_start-1] u [auxmap_end,next_end-1].  */
          if (next_start != auxmap_start)
            {
              if (next_start == curr_end)
                {
                  /* Merge adjacent segments.  */
                  curr_end = auxmap_start;
                }
              else
                {
                  if (curr_start < curr_end
                      && address >= curr_start && address <= curr_end-1)
                    {
                      if (next_end != auxmap_end)
                        {
                          vma->start = curr_start;
                          vma->end = curr_end;
#if STACK_DIRECTION < 0
                          vma->prev_end = prev_end;
#else
                          vma->next_start = auxmap_end;
#endif
                          goto found2;
                        }
                      else
                        goto found;
                    }
#if STACK_DIRECTION < 0
                  prev_end = curr_end;
#endif
                  curr_start = next_start; curr_end = auxmap_start;
                }
            }
          if (next_end != auxmap_end)
            {
              if (auxmap_end == curr_end)
                {
                  /* Merge adjacent segments.  */
                  curr_end = next_end;
                }
              else
                {
                  if (curr_start < curr_end
                      && address >= curr_start && address <= curr_end-1)
                    goto found;
#if STACK_DIRECTION < 0
                  prev_end = curr_end;
#endif
                  curr_start = auxmap_end; curr_end = next_end;
                }
            }
        }
      else
        {
          if (next_start == curr_end)
            {
              /* Merge adjacent segments.  */
              curr_end = next_end;
            }
          else
            {
              if (curr_start < curr_end
                  && address >= curr_start && address <= curr_end-1)
                goto found;
#if STACK_DIRECTION < 0
              prev_end = curr_end;
#endif
              curr_start = next_start; curr_end = next_end;
            }
        }
    }
  if (address >= curr_start && address <= curr_end-1)
    {
     found:
      vma->start = curr_start;
      vma->end = curr_end;
#if STACK_DIRECTION < 0
      vma->prev_end = prev_end;
#else
      if (rof_getchar (&rof) == '0'
          && rof_getchar (&rof) == 'x'
          && rof_scanf_lx (&rof, &vma->next_start) >= 0)
        {
          while (c = rof_peekchar (&rof), c == ' ' || c == '\t')
            rof_getchar (&rof);
          if (rof_getchar (&rof) == '0'
              && rof_getchar (&rof) == 'x'
              && rof_scanf_lx (&rof, &next_end) >= 0)
            ;
          else
            vma->next_start = 0;
        }
      else
        vma->next_start = 0;
#endif
     found2:
      rof_close (&rof);
      vma->is_near_this = simple_is_near_this;
      return 0;
    }
  rof_close (&rof);
 failed:
#if HAVE_MINCORE
  /* FreeBSD 6.[01] doesn't allow to distinguish unmapped pages from
     mapped but swapped-out pages.  See whether it's fixed.  */
  if (!is_mapped (0))
    /* OK, mincore() appears to work as expected.  */
    return mincore_get_vma (address, vma);
#endif
  return -1;
}
