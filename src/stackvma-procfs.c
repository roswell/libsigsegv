/* Determine the virtual memory area of a given address.
   Copyright (C) 2002, 2006, 2008-2009, 2016  Bruno Haible <bruno@clisp.org>

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
#include <unistd.h> /* open, close, read */
#include <errno.h> /* EINTR */
#include <fcntl.h> /* open */
#include <string.h> /* memcpy */
#include <sys/types.h>
#include <sys/stat.h> /* fstat */
#include <sys/mman.h> /* mmap, munmap */

/* Try to use the newer ("structured") /proc filesystem API, if supported.  */
#define _STRUCTURED_PROC 1
#include <sys/procfs.h> /* prmap_t, optionally PIOC* */

#include "stackvma-simple.c"

#if HAVE_MINCORE
# define sigsegv_get_vma mincore_get_vma
# define STATIC static
# include "stackvma-mincore.c"
# undef sigsegv_get_vma
#else
/* Cache for getpagesize().  */
static uintptr_t pagesize;
/* Initialize pagesize.  */
static void
init_pagesize (void)
{
  pagesize = getpagesize ();
}
#endif

static int
sigsegv_get_vma_fallback (uintptr_t address, struct vma_struct *vma)
{
#if HAVE_MINCORE
  return mincore_get_vma (address, vma);
#else
  return -1;
#endif
}

/* Note: Solaris <sys/procfs.h> defines a different type prmap_t with
   _STRUCTURED_PROC than without! Here's a table of sizeof(prmap_t):
                                32-bit   64-bit
       _STRUCTURED_PROC = 0       32       56
       _STRUCTURED_PROC = 1       96      104
   Therefore, if the include files provide the newer API, prmap_t has
   the bigger size, and thus you MUST use the newer API.  And if the
   include files provide the older API, prmap_t has the smaller size,
   and thus you MUST use the older API.  */

int
sigsegv_get_vma (uintptr_t address, struct vma_struct *vma)
{
#if defined PIOCNMAP && defined PIOCMAP
  /* We must use the older /proc interface.  */

  char fnamebuf[6+10+1];
  char *fname;
  int fd;
  int nmaps;
  size_t memneed;
#if HAVE_MMAP_ANON
# define zero_fd -1
# define map_flags MAP_ANON
#elif HAVE_MMAP_ANONYMOUS
# define zero_fd -1
# define map_flags MAP_ANONYMOUS
#else
  int zero_fd;
# define map_flags 0
#endif
  void *auxmap;
  uintptr_t auxmap_start;
  uintptr_t auxmap_end;
  prmap_t* maps;
  prmap_t* mp;
  uintptr_t start, end;
#if STACK_DIRECTION < 0
  uintptr_t prev;
#endif

  if (pagesize == 0)
    init_pagesize ();

  /* Construct fname = sprintf (fnamebuf+i, "/proc/%u", getpid ()).  */
  fname = fnamebuf + sizeof (fnamebuf) - 1;
  *fname = '\0';
  {
    unsigned int value = getpid ();
    do
      *--fname = (value % 10) + '0';
    while ((value = value / 10) > 0);
  }
  fname -= 6;
  memcpy (fname, "/proc/", 6);

  fd = open (fname, O_RDONLY);
  if (fd < 0)
    goto failed;

  if (ioctl (fd, PIOCNMAP, &nmaps) < 0)
    goto fail2;

  memneed = (nmaps + 10) * sizeof (prmap_t);
  /* Allocate memneed bytes of memory.
     We cannot use alloca here, because we are low on stack space.
     We also cannot use malloc here, because a malloc() call may have been
     interrupted.
     So use mmap(), and ignore the resulting VMA.  */
  memneed = ((memneed - 1) / pagesize + 1) * pagesize;
#if !(HAVE_MMAP_ANON || HAVE_MMAP_ANONYMOUS)
  zero_fd = open ("/dev/zero", O_RDONLY, 0644);
  if (zero_fd < 0)
    goto fail2;
#endif
  auxmap = (void *) mmap ((void *) 0, memneed, PROT_READ | PROT_WRITE, map_flags | MAP_PRIVATE, zero_fd, 0);
#if !(HAVE_MMAP_ANON || HAVE_MMAP_ANONYMOUS)
  close (zero_fd);
#endif
  if (auxmap == (void *) -1)
    goto fail2;
  auxmap_start = (uintptr_t) auxmap;
  auxmap_end = auxmap_start + memneed;
  maps = (prmap_t *) auxmap;

  if (ioctl (fd, PIOCMAP, maps) < 0)
    goto fail1;

#if STACK_DIRECTION < 0
  prev = 0;
#endif
  for (mp = maps;;)
    {
      start = (uintptr_t) mp->pr_vaddr;
      end = start + mp->pr_size;
      if (start == 0 && end == 0)
        break;
      mp++;
      if (start <= auxmap_start && auxmap_end - 1 <= end - 1)
        {
          /* Consider [start,end-1] \ [auxmap_start,auxmap_end-1]
             = [start,auxmap_start-1] u [auxmap_end,end-1].  */
          if (start != auxmap_start)
            {
              if (address >= start && address <= auxmap_start - 1)
                {
                  end = auxmap_start;
                  goto found;
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

 fail1:
  munmap (auxmap, memneed);
 fail2:
  close (fd);
 failed:
  return sigsegv_get_vma_fallback (address, vma);

 found:
  vma->start = start;
  vma->end = end;
#if STACK_DIRECTION < 0
  vma->prev_end = prev;
#else
  vma->next_start = (uintptr_t) mp->pr_vaddr;
#endif
  munmap (auxmap, memneed);
  close (fd);
  vma->is_near_this = simple_is_near_this;
  return 0;

#else
  /* We must use the newer /proc interface.
     Documentation:
     https://docs.oracle.com/cd/E23824_01/html/821-1473/proc-4.html
     The contents of /proc/<pid>/map consists of records of type
     prmap_t.  These are different in 32-bit and 64-bit processes,
     but here we are fortunately accessing only the current process.  */

  char fnamebuf[6+10+4+1];
  char *fname;
  int fd;
  int nmaps;
  size_t memneed;
#if HAVE_MMAP_ANON
# define zero_fd -1
# define map_flags MAP_ANON
#elif HAVE_MMAP_ANONYMOUS
# define zero_fd -1
# define map_flags MAP_ANONYMOUS
#else
  int zero_fd;
# define map_flags 0
#endif
  void *auxmap;
  uintptr_t auxmap_start;
  uintptr_t auxmap_end;
  prmap_t* maps;
  prmap_t* maps_end;
  prmap_t* mp;
  uintptr_t start, end;
#if STACK_DIRECTION < 0
  uintptr_t prev;
#endif

  if (pagesize == 0)
    init_pagesize ();

  /* Construct fname = sprintf (fnamebuf+i, "/proc/%u/map", getpid ()).  */
  fname = fnamebuf + sizeof (fnamebuf) - 1 - 4;
  memcpy (fname, "/map", 4 + 1);
  {
    unsigned int value = getpid ();
    do
      *--fname = (value % 10) + '0';
    while ((value = value / 10) > 0);
  }
  fname -= 6;
  memcpy (fname, "/proc/", 6);

  fd = open (fname, O_RDONLY);
  if (fd < 0)
    goto failed;

  {
    struct stat statbuf;
    if (fstat (fd, &statbuf) < 0)
      goto fail2;
    nmaps = statbuf.st_size / sizeof (prmap_t);
  }

  memneed = (nmaps + 10) * sizeof (prmap_t);
  /* Allocate memneed bytes of memory.
     We cannot use alloca here, because we are low on stack space.
     We also cannot use malloc here, because a malloc() call may have been
     interrupted.
     So use mmap(), and ignore the resulting VMA.  */
  memneed = ((memneed - 1) / pagesize + 1) * pagesize;
#if !(HAVE_MMAP_ANON || HAVE_MMAP_ANONYMOUS)
  zero_fd = open ("/dev/zero", O_RDONLY, 0644);
  if (zero_fd < 0)
    goto fail2;
#endif
  auxmap = (void *) mmap ((void *) 0, memneed, PROT_READ | PROT_WRITE, map_flags | MAP_PRIVATE, zero_fd, 0);
#if !(HAVE_MMAP_ANON || HAVE_MMAP_ANONYMOUS)
  close (zero_fd);
#endif
  if (auxmap == (void *) -1)
    goto fail2;
  auxmap_start = (uintptr_t) auxmap;
  auxmap_end = auxmap_start + memneed;
  maps = (prmap_t *) auxmap;

  /* Read up to memneed bytes from fd into maps.  */
  {
    size_t remaining = memneed;
    size_t total_read = 0;
    char *ptr = (char *) maps;

    do
      {
        size_t nread = read (fd, ptr, remaining);
        if (nread == (size_t)-1)
          {
            if (errno == EINTR)
              continue;
            goto fail1;
          }
        if (nread == 0)
          /* EOF */
          break;
        total_read += nread;
        ptr += nread;
        remaining -= nread;
      }
    while (remaining > 0);

    nmaps = (memneed - remaining) / sizeof (prmap_t);
    maps_end = maps + nmaps;
  }

#if STACK_DIRECTION < 0
  prev = 0;
#endif
  for (mp = maps; mp < maps_end; mp++)
    {
      start = (uintptr_t) mp->pr_vaddr;
      end = start + mp->pr_size;
      if (start <= auxmap_start && auxmap_end - 1 <= end - 1)
        {
          /* Consider [start,end-1] \ [auxmap_start,auxmap_end-1]
             = [start,auxmap_start-1] u [auxmap_end,end-1].  */
          if (start != auxmap_start)
            {
              if (address >= start && address <= auxmap_start - 1)
                {
                  end = auxmap_start;
                  goto found;
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

 fail1:
  munmap (auxmap, memneed);
 fail2:
  close (fd);
 failed:
  return sigsegv_get_vma_fallback (address, vma);

 found:
  vma->start = start;
  vma->end = end;
#if STACK_DIRECTION < 0
  vma->prev_end = prev;
#else
  vma->next_start = (uintptr_t) mp->pr_vaddr;
#endif
  munmap (auxmap, memneed);
  close (fd);
  vma->is_near_this = simple_is_near_this;
  return 0;
#endif
}
