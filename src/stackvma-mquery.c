/* Determine the virtual memory area of a given address.
   Copyright (C) 2011  Bruno Haible <bruno@clisp.org>

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

/* mquery() is a system call that allows to inquire the status of a
   range of pages of virtual memory.  In particular, it allows to inquire
   whether a page is mapped at all, and where is the next unmapped page
   after a given address.
   As of 2011, mquery() is supported by:
     - OpenBSD, since OpenBSD 3.4.
   Note that this file can give different results.  For example, on
   OpenBSD 4.4 / i386 the stack segment (which starts around 0xcdbfe000)
   ends at 0xcfbfdfff according to mincore, but at 0xffffffff according to
   mquery.  */

#include "stackvma.h"
#include <limits.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/mman.h>
#include <stdlib.h>

#if DEBUG_COMPARE_MQUERY_AND_MINCORE
# include <stdio.h>
# define is_mapped mincore_is_mapped
# define mapped_range_start mincore_mapped_range_start
# define mapped_range_end mincore_mapped_range_end
# define is_unmapped mincore_is_unmapped
# define sigsegv_get_vma mincore_get_vma
# define STATIC static
# include "stackvma-mincore.c"
# undef is_mapped
# undef mapped_range_start
# undef mapped_range_end
# undef is_unmapped
# undef sigsegv_get_vma
# define is_mapped mquery_is_mapped
# define mapped_range_start mquery_mapped_range_start
# define mapped_range_end mquery_mapped_range_end
# define is_unmapped mquery_is_unmapped
#else

/* Cache for getpagesize().  */
static unsigned long pagesize;

/* Initialize pagesize.  */
static void
init_pagesize (void)
{
#if HAVE_GETPAGESIZE
  pagesize = getpagesize ();
#elif HAVE_SYSCONF_PAGESIZE
  pagesize = sysconf (_SC_PAGESIZE);
#else
  pagesize = PAGESIZE;
#endif
}

#endif

/* Test whether the page starting at ADDR is among the address range.
   ADDR must be a multiple of pagesize.  */
static int
is_mapped (unsigned long addr)
{
  /* Avoid calling mquery with a NULL first argument, because this argument
     value has a specific meaning.  We know the NULL page is unmapped.  */
  if (addr == 0)
    return 0;
  return mquery ((void *) addr, pagesize, 0, MAP_FIXED, -1, 0) == (void *) -1;
}

/* Assuming that the page starting at ADDR is among the address range,
   return the start of its virtual memory range.
   ADDR must be a multiple of pagesize.  */
static unsigned long
mapped_range_start (unsigned long addr)
{
  unsigned long stepsize;
  unsigned long known_unmapped_page;

  /* Look at smaller addresses, in larger and larger steps, to minimize the
     number of mquery() calls.  */
  stepsize = pagesize;
  for (;;)
    {
      unsigned long hole;

      if (addr == 0)
        abort ();

      if (addr <= stepsize)
        {
          known_unmapped_page = 0;
          break;
        }

      hole = (unsigned long) mquery ((void *) (addr - stepsize), pagesize,
                                     0, 0, -1, 0);
      if (!(hole == (unsigned long) (void *) -1 || hole >= addr))
        {
          /* Some part of [addr - stepsize, addr - 1] is unmapped.  */
          known_unmapped_page = hole;
          break;
        }

      /* The entire range [addr - stepsize, addr - 1] is mapped.  */
      addr -= stepsize;

      if (2 * stepsize > stepsize && 2 * stepsize < addr)
        stepsize = 2 * stepsize;
    }

  /* Now reduce the step size again.
     We know that the page at known_unmapped_page is unmapped and that
     0 < addr - known_unmapped_page <= stepsize.  */
  while (stepsize > pagesize && stepsize / 2 >= addr - known_unmapped_page)
    stepsize = stepsize / 2;
  /* Still 0 < addr - known_unmapped_page <= stepsize.  */
  while (stepsize > pagesize)
    {
      unsigned long hole;

      stepsize = stepsize / 2;
      hole = (unsigned long) mquery ((void *) (addr - stepsize), pagesize,
                                     0, 0, -1, 0);
      if (!(hole == (unsigned long) (void *) -1 || hole >= addr))
        /* Some part of [addr - stepsize, addr - 1] is unmapped.  */
        known_unmapped_page = hole;
      else
        /* The entire range [addr - stepsize, addr - 1] is mapped.  */
        addr -= stepsize;
      /* Still 0 < addr - known_unmapped_page <= stepsize.  */
    }

  return addr;
}

/* Assuming that the page starting at ADDR is among the address range,
   return the end of its virtual memory range + 1.
   ADDR must be a multiple of pagesize.  */
static unsigned long
mapped_range_end (unsigned long addr)
{
  unsigned long end;

  if (addr == 0)
    abort ();

  end = (unsigned long) mquery ((void *) addr, pagesize, 0, 0, -1, 0);
  if (end == (unsigned long) (void *) -1)
    end = 0; /* wrap around */
  return end;
}

/* Determine whether an address range [ADDR1..ADDR2] is completely unmapped.
   ADDR1 must be <= ADDR2.  */
static int
is_unmapped (unsigned long addr1, unsigned long addr2)
{
  /* Round addr1 down.  */
  addr1 = (addr1 / pagesize) * pagesize;
  /* Round addr2 up and turn it into an exclusive bound.  */
  addr2 = ((addr2 / pagesize) + 1) * pagesize;

  /* Avoid calling mquery with a NULL first argument, because this argument
     value has a specific meaning.  We know the NULL page is unmapped.  */
  if (addr1 == 0)
    addr1 = pagesize;

  if (addr1 < addr2)
    {
      if (mquery ((void *) addr1, addr2 - addr1, 0, MAP_FIXED, -1, 0)
          == (void *) -1)
        /* Not all the interval [addr1 .. addr2 - 1] is unmapped.  */
        return 0;
      else
        /* The interval [addr1 .. addr2 - 1] is unmapped.  */
        return 1;
    }
  return 1;
}

#if DEBUG_COMPARE_MQUERY_AND_MINCORE

# undef is_mapped
# undef mapped_range_start
# undef mapped_range_end
# undef is_unmapped

static int
is_mapped (unsigned long addr)
{
  int result1 = mincore_is_mapped (addr);
  int result2 = mquery_is_mapped (addr);
  if (result1 != result2)
    {
      fprintf (stderr, "is_mapped(0x%lx) = %d %d\n", addr, result1, result2);
      fflush (stderr);
    }
  return result2;
}

static unsigned long
mapped_range_start (unsigned long addr)
{
  unsigned long result1 = mincore_mapped_range_start (addr);
  unsigned long result2 = mquery_mapped_range_start (addr);
  if (result1 != result2)
    {
      fprintf (stderr, "mapped_range_start(0x%lx) = 0x%lx 0x%lx\n",
               addr, result1, result2);
      fflush (stderr);
    }
  return result2;
}

static unsigned long
mapped_range_end (unsigned long addr)
{
  unsigned long result1 = mincore_mapped_range_end (addr);
  unsigned long result2 = mquery_mapped_range_end (addr);
  if (result1 != result2)
    {
      fprintf (stderr, "mapped_range_end(0x%lx) = 0x%lx 0x%lx\n",
               addr, result1, result2);
      fflush (stderr);
    }
  return result2;
}

static int
is_unmapped (unsigned long addr1, unsigned long addr2)
{
  int result1 = mincore_is_unmapped (addr1, addr2);
  int result2 = mquery_is_unmapped (addr1, addr2);
  if (result1 != result2)
    {
      fprintf (stderr, "is_unmapped(0x%lx,0x%lx) = %d %d\n",
               addr1, addr2, result1, result2);
      fflush (stderr);
    }
  return result2;
}

#endif

#if STACK_DIRECTION < 0

/* Info about the gap between this VMA and the previous one.
   addr must be < vma->start.  */
static int
mquery_is_near_this (unsigned long addr, struct vma_struct *vma)
{
  /*   vma->start - addr <= (vma->start - vma->prev_end) / 2
     is mathematically equivalent to
       vma->prev_end <= 2 * addr - vma->start
     <==> is_unmapped (2 * addr - vma->start, vma->start - 1).
     But be careful about overflow: if 2 * addr - vma->start is negative,
     we consider a tiny "guard page" mapping [0, 0] to be present around
     NULL; it intersects the range (2 * addr - vma->start, vma->start - 1),
     therefore return false.  */
  unsigned long testaddr = addr - (vma->start - addr);
  if (testaddr > addr) /* overflow? */
    return 0;
  /* Here testaddr <= addr < vma->start.  */
  return is_unmapped (testaddr, vma->start - 1);
}

#endif
#if STACK_DIRECTION > 0

/* Info about the gap between this VMA and the next one.
   addr must be > vma->end - 1.  */
static int
mquery_is_near_this (unsigned long addr, struct vma_struct *vma)
{
  /*   addr - vma->end < (vma->next_start - vma->end) / 2
     is mathematically equivalent to
       vma->next_start > 2 * addr - vma->end
     <==> is_unmapped (vma->end, 2 * addr - vma->end).
     But be careful about overflow: if 2 * addr - vma->end is > ~0UL,
     we consider a tiny "guard page" mapping [0, 0] to be present around
     NULL; it intersects the range (vma->end, 2 * addr - vma->end),
     therefore return false.  */
  unsigned long testaddr = addr + (addr - vma->end);
  if (testaddr < addr) /* overflow? */
    return 0;
  /* Here vma->end - 1 < addr <= testaddr.  */
  return is_unmapped (vma->end, testaddr);
}

#endif

int
sigsegv_get_vma (unsigned long address, struct vma_struct *vma)
{
  if (pagesize == 0)
    init_pagesize ();
  address = (address / pagesize) * pagesize;
  vma->start = mapped_range_start (address);
  vma->end = mapped_range_end (address);
  vma->is_near_this = mquery_is_near_this;
  return 0;
}
