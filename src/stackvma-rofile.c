/* Buffered read-only streams.
   Copyright (C) 2008, 2016-2017  Bruno Haible <bruno@clisp.org>

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

#include <errno.h> /* errno, EINTR */
#include <fcntl.h> /* O_RDONLY */
#include <stddef.h> /* size_t */
#include <unistd.h> /* getpagesize, lseek, read, close */
#include <sys/types.h>
#include <sys/mman.h> /* mmap, munmap */

#if defined __linux__
# include <limits.h> /* PATH_MAX */
#endif

/* DragonFly BSD 3.8 still has only MAP_ANON and not MAP_ANONYMOUS.  */
#if HAVE_MMAP_ANON && !HAVE_MMAP_ANONYMOUS
# define MAP_ANONYMOUS MAP_ANON
#endif

/* Buffered read-only streams.
   We cannot use <stdio.h> here, because fopen() calls malloc(), and a malloc()
   call may have been interrupted.
   Also, we cannot use multiple read() calls, because if the buffer size is
   smaller than the file's contents:
     - On NetBSD, the second read() call would return 0, thus making the file
       appear truncated.
     - On DragonFly BSD, the first read() call would fail with errno = EFBIG.
     - On all platforms, if some other thread is doing memory allocations or
       deallocations between two read() calls, there is a high risk that the
       result of these two read() calls don't fit together, and as a
       consequence we will parse gargage and either omit some VMAs or return
       VMAs with nonsensical addresses.
   So use mmap(), and ignore the resulting VMA.
   The stack-allocated buffer cannot be too large, because this can be called
   when we are in the context of an alternate stack of just SIGSTKSZ bytes.  */

#ifdef __linux__
  /* On Linux, if the file does not entirely fit into the buffer, the read()
     function stops before the line that would come out truncated.  The
     maximum size of such a line is 73 + PATH_MAX bytes.  To be sure that we
     have read everything, we must verify that at least that many bytes are
     left when read() returned.  */
# define MIN_LEFTOVER (73 + PATH_MAX)
#else
# define MIN_LEFTOVER 1
#endif

#if MIN_LEFTOVER < 1024
# define STACK_ALLOCATED_BUFFER_SIZE 1024
#else
  /* There is no point in using a stack-allocated buffer if it is too small
     anyway.  */
# define STACK_ALLOCATED_BUFFER_SIZE 1
#endif

struct rofile
  {
    size_t position;
    size_t filled;
    int eof_seen;
    /* These fields deal with allocation of the buffer.  */
    char *buffer;
    char *auxmap;
    size_t auxmap_length;
    uintptr_t auxmap_start;
    uintptr_t auxmap_end;
    char stack_allocated_buffer[STACK_ALLOCATED_BUFFER_SIZE];
  };

/* Open a read-only file stream.  */
static int
rof_open (struct rofile *rof, const char *filename)
{
  int fd;
  uintptr_t pagesize;
  size_t size;

  fd = open (filename, O_RDONLY);
  if (fd < 0)
    return -1;
  rof->position = 0;
  rof->eof_seen = 0;
  /* Try the static buffer first.  */
  pagesize = 0;
  rof->buffer = rof->stack_allocated_buffer;
  size = sizeof (rof->stack_allocated_buffer);
  rof->auxmap = NULL;
  rof->auxmap_start = 0;
  rof->auxmap_end = 0;
  for (;;)
    {
      /* Attempt to read the contents in a single system call.  */
      if (size > MIN_LEFTOVER)
        {
          int n = read (fd, rof->buffer, size);
          if (n < 0 && errno == EINTR)
            goto retry;
#if defined __DragonFly__
          if (!(n < 0 && errno == EFBIG))
#endif
            {
              if (n <= 0)
                /* Empty file.  */
                goto fail1;
              if (n + MIN_LEFTOVER <= size)
                {
                  /* The buffer was sufficiently large.  */
                  rof->filled = n;
#ifdef __linux__
                  /* On Linux, the read() call may stop even if the buffer was
                     large enough.  We need the equivalent of full_read().  */
                  for (;;)
                    {
                      n = read (fd, rof->buffer + rof->filled, size - rof->filled);
                      if (n < 0 && errno == EINTR)
                        goto retry;
                      if (n < 0)
                        /* Some error.  */
                        goto fail1;
                      if (n + MIN_LEFTOVER > size - rof->filled)
                        /* Allocate a larger buffer.  */
                        break;
                      if (n == 0)
                        {
                          /* Reached the end of file.  */
                          close (fd);
                          return 0;
                        }
                      rof->filled += n;
                    }
#else
                  close (fd);
                  return 0;
#endif
                }
            }
        }
      /* Allocate a larger buffer.  */
      if (pagesize == 0)
        {
          pagesize = getpagesize ();
          size = pagesize;
          while (size <= MIN_LEFTOVER)
            size = 2 * size;
        }
      else
        {
          size = 2 * size;
          if (size == 0)
            /* Wraparound.  */
            goto fail1;
          if (rof->auxmap != NULL)
            munmap (rof->auxmap, rof->auxmap_length);
        }
      rof->auxmap = (void *) mmap ((void *) 0, size, PROT_READ | PROT_WRITE,
                                   MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
      if (rof->auxmap == (void *) -1)
        {
          close (fd);
          return -1;
        }
      rof->auxmap_length = size;
      rof->auxmap_start = (uintptr_t) rof->auxmap;
      rof->auxmap_end = rof->auxmap_start + size;
      rof->buffer = (char *) rof->auxmap;
     retry:
      /* Restart.  */
      if (lseek (fd, 0, SEEK_SET) < 0)
        {
          close (fd);
          fd = open (filename, O_RDONLY);
          if (fd < 0)
            goto fail2;
        }
    }
 fail1:
  close (fd);
 fail2:
  if (rof->auxmap != NULL)
    munmap (rof->auxmap, rof->auxmap_length);
  return -1;
}

/* Return the next byte from a read-only file stream without consuming it,
   or -1 at EOF.  */
static int
rof_peekchar (struct rofile *rof)
{
  if (rof->position == rof->filled)
    {
      rof->eof_seen = 1;
      return -1;
    }
  return (unsigned char) rof->buffer[rof->position];
}

/* Return the next byte from a read-only file stream, or -1 at EOF.  */
static int
rof_getchar (struct rofile *rof)
{
  int c = rof_peekchar (rof);
  if (c >= 0)
    rof->position++;
  return c;
}

/* Parse an unsigned hexadecimal number from a read-only file stream.  */
static int
rof_scanf_lx (struct rofile *rof, uintptr_t *valuep)
{
  uintptr_t value = 0;
  unsigned int numdigits = 0;
  for (;;)
    {
      int c = rof_peekchar (rof);
      if (c >= '0' && c <= '9')
        value = (value << 4) + (c - '0');
      else if (c >= 'A' && c <= 'F')
        value = (value << 4) + (c - 'A' + 10);
      else if (c >= 'a' && c <= 'f')
        value = (value << 4) + (c - 'a' + 10);
      else
        break;
      rof_getchar (rof);
      numdigits++;
    }
  if (numdigits == 0)
    return -1;
  *valuep = value;
  return 0;
}

/* Close a read-only file stream.  */
static void
rof_close (struct rofile *rof)
{
  if (rof->auxmap != NULL)
    munmap (rof->auxmap, rof->auxmap_length);
}
