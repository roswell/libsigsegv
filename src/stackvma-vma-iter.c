/* Iterate through the virtual memory areas of the current process,
   by reading from the /proc file system.
   Copyright (C) 2002-2017  Bruno Haible <bruno@clisp.org>

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

/* This code is a simplied copy (no handling of protection flags) of the
   code in gnulib's lib/vma-iter.c.  */

/* Iterate over the virtual memory areas of the current process.
   If such iteration is supported, the callback is called once for every
   virtual memory area, in ascending order, with the following arguments:
     - LOCALS is the same argument as passed to vma_iterate.
     - START is the address of the first byte in the area, page-aligned.
     - END is the address of the last byte in the area plus 1, page-aligned.
       Note that it may be 0 for the last area in the address space.
   If the callback returns 0, the iteration continues.  If it returns 1,
   the iteration terminates prematurely.
   This function may open file descriptors, but does not call malloc().
   Return 0 if all went well, or -1 in case of error.  */
static int
vma_iterate (struct callback_locals *locals)
{
#if defined __linux__ || (defined __FreeBSD_kernel__ && !defined __FreeBSD__) /* || defined __CYGWIN__ */
  /* GNU/kFreeBSD mounts /proc as linprocfs, which looks like a Linux /proc
     file system.  */

  struct rofile rof;

  /* Open the current process' maps file.  It describes one VMA per line.  */
  if (rof_open (&rof, "/proc/self/maps") >= 0)
    {
      uintptr_t auxmap_start = rof.auxmap_start;
      uintptr_t auxmap_end = rof.auxmap_end;

      for (;;)
        {
          uintptr_t start, end;
          int c;

          /* Parse one line.  First start and end.  */
          if (!(rof_scanf_lx (&rof, &start) >= 0
                && rof_getchar (&rof) == '-'
                && rof_scanf_lx (&rof, &end) >= 0))
            break;
          while (c = rof_getchar (&rof), c != -1 && c != '\n')
            ;

          if (start <= auxmap_start && auxmap_end - 1 <= end - 1)
            {
              /* Consider [start,end-1] \ [auxmap_start,auxmap_end-1]
                 = [start,auxmap_start-1] u [auxmap_end,end-1].  */
              if (start < auxmap_start)
                if (callback (locals, start, auxmap_start))
                  break;
              if (auxmap_end - 1 < end - 1)
                if (callback (locals, auxmap_end, end))
                  break;
            }
          else
            {
              if (callback (locals, start, end))
                break;
            }
        }
      rof_close (&rof);
      return 0;
    }

  return -1;

#elif defined __FreeBSD__ || defined __DragonFly__ || defined __NetBSD__

  struct rofile rof;

  /* Open the current process' maps file.  It describes one VMA per line.
     On FreeBSD:
       Cf. <http://www.freebsd.org/cgi/cvsweb.cgi/src/sys/fs/procfs/procfs_map.c?annotate=HEAD>
     On NetBSD, there are two such files:
       - /proc/curproc/map in near-FreeBSD syntax,
       - /proc/curproc/maps in Linux syntax.
       Cf. <http://cvsweb.netbsd.org/bsdweb.cgi/src/sys/miscfs/procfs/procfs_map.c?rev=HEAD> */
  if (rof_open (&rof, "/proc/curproc/map") >= 0)
    {
      uintptr_t auxmap_start = rof.auxmap_start;
      uintptr_t auxmap_end = rof.auxmap_end;

      for (;;)
        {
          uintptr_t start, end;
          int c;

          /* Parse one line.  First start.  */
          if (!(rof_getchar (&rof) == '0'
                && rof_getchar (&rof) == 'x'
                && rof_scanf_lx (&rof, &start) >= 0))
            break;
          while (c = rof_peekchar (&rof), c == ' ' || c == '\t')
            rof_getchar (&rof);
          /* Then end.  */
          if (!(rof_getchar (&rof) == '0'
                && rof_getchar (&rof) == 'x'
                && rof_scanf_lx (&rof, &end) >= 0))
            break;
          while (c = rof_getchar (&rof), c != -1 && c != '\n')
            ;

          if (start <= auxmap_start && auxmap_end - 1 <= end - 1)
            {
              /* Consider [start,end-1] \ [auxmap_start,auxmap_end-1]
                 = [start,auxmap_start-1] u [auxmap_end,end-1].  */
              if (start < auxmap_start)
                if (callback (locals, start, auxmap_start))
                  break;
              if (auxmap_end - 1 < end - 1)
                if (callback (locals, auxmap_end, end))
                  break;
            }
          else
            {
              if (callback (locals, start, end))
                break;
            }
        }
      rof_close (&rof);
      return 0;
    }

  return -1;

#else

  /* Not implemented.  */
  return -1;

#endif
}
