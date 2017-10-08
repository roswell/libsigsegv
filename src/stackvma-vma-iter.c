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


#if defined __linux__ || (defined __FreeBSD_kernel__ && !defined __FreeBSD__) /* || defined __CYGWIN__ */
/* GNU/kFreeBSD mounts /proc as linprocfs, which looks like a Linux /proc
   file system.  */

static int
vma_iterate_proc (struct callback_locals *locals)
{
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
}

#elif defined __FreeBSD__ || defined __DragonFly__ || defined __NetBSD__

static int
vma_iterate_proc (struct callback_locals *locals)
{
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
}

#endif


#if (defined __FreeBSD__ || defined __FreeBSD_kernel__) && defined KERN_PROC_VMMAP /* FreeBSD >= 7.1 */

static int
vma_iterate_bsd (struct callback_locals *locals)
{
  /* Documentation: https://www.freebsd.org/cgi/man.cgi?sysctl(3)  */
  int info_path[] = { CTL_KERN, KERN_PROC, KERN_PROC_VMMAP, getpid () };
  size_t len;
  size_t pagesize;
  size_t memneed;
  void *auxmap;
  unsigned long auxmap_start;
  unsigned long auxmap_end;
  char *mem;
  char *p;
  char *p_end;

  len = 0;
  if (sysctl (info_path, 4, NULL, &len, NULL, 0) < 0)
    return -1;
  /* Allow for small variations over time.  In a multithreaded program
     new VMAs can be allocated at any moment.  */
  len = 2 * len + 200;
  /* Allocate memneed bytes of memory.
     We cannot use alloca here, because not much stack space is guaranteed.
     We also cannot use malloc here, because a malloc() call may call mmap()
     and thus pre-allocate available memory.
     So use mmap(), and ignore the resulting VMA.  */
  pagesize = getpagesize ();
  memneed = len;
  memneed = ((memneed - 1) / pagesize + 1) * pagesize;
  auxmap = (void *) mmap ((void *) 0, memneed, PROT_READ | PROT_WRITE,
                          MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (auxmap == (void *) -1)
    return -1;
  auxmap_start = (unsigned long) auxmap;
  auxmap_end = auxmap_start + memneed;
  mem = (char *) auxmap;
  if (sysctl (info_path, 4, mem, &len, NULL, 0) < 0)
    {
      munmap (auxmap, memneed);
      return -1;
    }
  p = mem;
  p_end = mem + len;
  while (p < p_end)
    {
      struct kinfo_vmentry *kve = (struct kinfo_vmentry *) p;
      unsigned long start = kve->kve_start;
      unsigned long end = kve->kve_end;
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
      p += kve->kve_structsize;
    }
  munmap (auxmap, memneed);
  return 0;
}

#else

# define vma_iterate_bsd(locals) (-1)

#endif


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
#if defined __linux__ || defined __FreeBSD_kernel__ || defined __FreeBSD__ || defined __DragonFly__ || defined __NetBSD__ /* || defined __CYGWIN__ */

# if defined __FreeBSD__
  /* On FreeBSD with procfs (but not GNU/kFreeBSD, which uses linprocfs), the
     function vma_iterate_proc does not return the virtual memory areas that
     were created by anonymous mmap.  See
     <https://svnweb.freebsd.org/base/head/sys/fs/procfs/procfs_map.c?view=markup>
     So use vma_iterate_proc only as a fallback.  */
  int retval = vma_iterate_bsd (locals);
  if (retval == 0)
      return 0;

  return vma_iterate_proc (locals);
# else
  /* On the other platforms, try the /proc approach first, and the sysctl()
     as a fallback.  */
  int retval = vma_iterate_proc (locals);
  if (retval == 0)
      return 0;

  return vma_iterate_bsd (locals);
# endif

#else

  /* Not implemented.  */
  return -1;

#endif
}
