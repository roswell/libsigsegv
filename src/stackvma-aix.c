/* Determine the virtual memory area of a given address.  AIX version.
   Copyright (C) 2021-2022  Bruno Haible <bruno@clisp.org>

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

#include <unistd.h> /* getpagesize, getpid, close, read */
#include <errno.h> /* EINTR */
#include <fcntl.h> /* open */
#include <string.h> /* memcpy */
#include <sys/types.h>
#include <sys/mman.h> /* mmap, munmap */
#include <sys/procfs.h> /* prmap_t */
#include <sys/utsname.h> /* uname */

#include "stackvma-simple.c"

#include "stackvma-mincore.c"

struct callback_locals
{
  uintptr_t address;
  struct vma_struct *vma;
#if STACK_DIRECTION < 0
  uintptr_t prev;
#else
  int stop_at_next_vma;
#endif
  int retval;
};

static int
callback (struct callback_locals *locals, uintptr_t start, uintptr_t end)
{
#if STACK_DIRECTION < 0
  if (locals->address >= start && locals->address <= end - 1)
    {
      locals->vma->start = start;
      locals->vma->end = end;
      locals->vma->prev_end = locals->prev;
      locals->retval = 0;
      return 1;
    }
  locals->prev = end;
#else
  if (locals->stop_at_next_vma)
    {
      locals->vma->next_start = start;
      locals->stop_at_next_vma = 0;
      return 1;
    }
  if (locals->address >= start && locals->address <= end - 1)
    {
      locals->vma->start = start;
      locals->vma->end = end;
      locals->retval = 0;
      locals->stop_at_next_vma = 1;
      return 0;
    }
#endif
  return 0;
}

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
/* This code is a simplified copy (no handling of protection flags) of the
   code in gnulib's lib/vma-iter.c.  */
static int
vma_iterate (struct callback_locals *locals)
{
  /* On AIX, there is a /proc/$pic/map file, that contains records of type
     prmap_t, defined in <sys/procfs.h>.  In older versions of AIX, it lists
     only the virtual memory areas that are connected to a file, not the
     anonymous ones.  But at least since AIX 7.1, it is well usable.  */

  char fnamebuf[6+10+4+1];
  char *fname;
  int fd;
  size_t memneed;

  if (pagesize == 0)
    init_pagesize ();

  /* Construct fname = sprintf (fnamebuf+i, "/proc/%u/map", getpid ()).  */
  fname = fnamebuf + sizeof (fnamebuf) - (4+1);
  memcpy (fname, "/map", 4+1);
  {
    unsigned int value = getpid ();
    do
      *--fname = (value % 10) + '0';
    while ((value = value / 10) > 0);
  }
  fname -= 6;
  memcpy (fname, "/proc/", 6);

  fd = open (fname, O_RDONLY | O_CLOEXEC);
  if (fd < 0)
    return -1;

  /* The contents of /proc/<pid>/map contains a number of prmap_t entries,
     then an entirely null prmap_t entry, then a heap of NUL terminated
     strings.
     Documentation: https://www.ibm.com/docs/en/aix/7.1?topic=files-proc-file
     We read the entire contents, but look only at the prmap_t entries and
     ignore the tail part.  */

  for (memneed = 2 * pagesize; ; memneed = 2 * memneed)
    {
      /* Allocate memneed bytes of memory.
         We cannot use alloca here, because not much stack space is guaranteed.
         We also cannot use malloc here, because a malloc() call may call mmap()
         and thus pre-allocate available memory.
         So use mmap(), and ignore the resulting VMA if it occurs among the
         resulting VMAs.  (Normally it doesn't, because it was allocated after
         the open() call.)  */
      void *auxmap;
      unsigned long auxmap_start;
      unsigned long auxmap_end;
      ssize_t nbytes;

      auxmap = (void *) mmap ((void *) 0, memneed, PROT_READ | PROT_WRITE,
                              MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
      if (auxmap == (void *) -1)
        {
          close (fd);
          return -1;
        }
      auxmap_start = (unsigned long) auxmap;
      auxmap_end = auxmap_start + memneed;

      /* Read the contents of /proc/<pid>/map in a single system call.
         This guarantees a consistent result (no duplicated or omitted
         entries).  */
     retry:
      do
        nbytes = read (fd, auxmap, memneed);
      while (nbytes < 0 && errno == EINTR);
      if (nbytes <= 0)
        {
          munmap (auxmap, memneed);
          close (fd);
          return -1;
        }
      if (nbytes == memneed)
        {
          /* Need more memory.  */
          munmap (auxmap, memneed);
          if (lseek (fd, 0, SEEK_SET) < 0)
            {
              close (fd);
              return -1;
            }
        }
      else
        {
          if (read (fd, (char *) auxmap + nbytes, 1) > 0)
            {
              /* Oops, we had a short read.  Retry.  */
              if (lseek (fd, 0, SEEK_SET) < 0)
                {
                  munmap (auxmap, memneed);
                  close (fd);
                  return -1;
                }
              goto retry;
            }

          /* We now have the entire contents of /proc/<pid>/map in memory.  */
          prmap_t* maps = (prmap_t *) auxmap;

          /* The entries are not sorted by address.  Therefore
             1. Extract the relevant information into an array.
             2. Sort the array in ascending order.
             3. Invoke the callback.  */
          typedef struct
            {
              uintptr_t start;
              uintptr_t end;
            }
          vma_t;
          /* Since 2 * sizeof (vma_t) <= sizeof (prmap_t), we can reuse the
             same memory.  */
          vma_t *vmas = (vma_t *) auxmap;

          vma_t *vp = vmas;
          {
            prmap_t* mp;
            for (mp = maps;;)
              {
                unsigned long start, end;

                start = (unsigned long) mp->pr_vaddr;
                end = start + mp->pr_size;
                if (start == 0 && end == 0 && mp->pr_mflags == 0)
                  break;
                /* Discard empty VMAs and kernel VMAs.  */
                if (start < end && (mp->pr_mflags & MA_KERNTEXT) == 0)
                  {
                    if (start <= auxmap_start && auxmap_end - 1 <= end - 1)
                      {
                        /* Consider [start,end-1] \ [auxmap_start,auxmap_end-1]
                           = [start,auxmap_start-1] u [auxmap_end,end-1].  */
                        if (start < auxmap_start)
                          {
                            vp->start = start;
                            vp->end = auxmap_start;
                            vp++;
                          }
                        if (auxmap_end - 1 < end - 1)
                          {
                            vp->start = auxmap_end;
                            vp->end = end;
                            vp++;
                          }
                      }
                    else
                      {
                        vp->start = start;
                        vp->end = end;
                        vp++;
                      }
                  }
                mp++;
              }
          }

          size_t nvmas = vp - vmas;
          /* Sort the array in ascending order.
             Better not call qsort(), since it may call malloc().
             Insertion-sort is OK in this case, despite its worst-case running
             time of O(N²), since the number of VMAs will rarely be larger than
             1000.  */
          {
            size_t i;
            for (i = 1; i < nvmas; i++)
              {
                /* Invariant: Here vmas[0..i-1] is sorted.  */
                size_t j;
                for (j = i; j > 0 && vmas[j - 1].start > vmas[j].start; j--)
                  {
                    vma_t tmp = vmas[j - 1];
                    vmas[j - 1] = vmas[j];
                    vmas[j] = tmp;
                  }
                /* Invariant: Here vmas[0..i] is sorted.  */
              }
          }

          /* Invoke the callback.  */
          {
            size_t i;
            for (i = 0; i < nvmas; i++)
              {
                vma_t *vpi = &vmas[i];
                if (callback (locals, vpi->start, vpi->end))
                  break;
              }
          }

          munmap (auxmap, memneed);
          break;
        }
    }

  close (fd);
  return 0;
}

int
sigsegv_get_vma (uintptr_t address, struct vma_struct *vma)
{
  struct utsname u;
  if (uname (&u) >= 0
      /* && strcmp (u.sysname, "AIX") == 0 */
      && !(u.version[0] >= '1' && u.version[0] <= '6' && u.version[1] == '\0'))
    {
      /* AIX 7 or higher.  */
      struct callback_locals locals;
      locals.address = address;
      locals.vma = vma;
#if STACK_DIRECTION < 0
      locals.prev = 0;
#else
      locals.stop_at_next_vma = 0;
#endif
      locals.retval = -1;

      vma_iterate (&locals);
      if (locals.retval == 0)
        {
#if !(STACK_DIRECTION < 0)
          if (locals.stop_at_next_vma)
            vma->next_start = 0;
#endif
          vma->is_near_this = simple_is_near_this;
          return 0;
        }
    }

  return mincore_get_vma (address, vma);
}
