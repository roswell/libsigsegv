# mmap-anon.m4
# serial 4 (libsigsegv-2.9)
dnl Copyright (C) 2002-2024 Bruno Haible <bruno@clisp.org>
dnl This file is free software, distributed under the terms of the GNU
dnl General Public License.  As a special exception to the GNU General
dnl Public License, this file may be distributed as part of a program
dnl that contains a configuration script generated by Autoconf, under
dnl the same distribution terms as the rest of that program.

# How to allocate fresh memory using mmap.
AC_DEFUN([SV_MMAP_ANON],
[
  AC_REQUIRE([AC_PROG_CC])
  AC_REQUIRE([AC_CANONICAL_HOST])

  dnl 1) MAP_ANON

  AC_CACHE_CHECK([for mmap with MAP_ANON], [sv_cv_func_mmap_anon], [
    AC_RUN_IFELSE([
      AC_LANG_SOURCE([[
#include <sys/types.h>
#include <sys/mman.h>
int main ()
{
  void *p = mmap (0, 0x10000, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
  return (p == (void *)(-1));
}]])],
      [sv_cv_func_mmap_anon=yes],
      [sv_cv_func_mmap_anon=no],
      [
        dnl FIXME: Put in some more known values here.
        case "$host_os" in
          freebsd* | linux* | osf* | darwin*)
            sv_cv_func_mmap_anon=yes ;;
          *)
            AC_LINK_IFELSE([
              AC_LANG_PROGRAM([[
#include <sys/types.h>
#include <sys/mman.h>
]],
                [[mmap (0, 0x10000, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);]])],
              [sv_cv_func_mmap_anon="guessing yes"],
              [sv_cv_func_mmap_anon=no])
            ;;
        esac
      ])
  ])
  if test "$sv_cv_func_mmap_anon" != no; then
    AC_DEFINE([HAVE_MMAP_ANON], [1],
      [Define if <sys/mman.h> defines MAP_ANON and mmaping with MAP_ANON works.])
  fi

  dnl 2) MAP_ANONYMOUS

  AC_CACHE_CHECK([for mmap with MAP_ANONYMOUS], [sv_cv_func_mmap_anonymous], [
    AC_RUN_IFELSE([
      AC_LANG_SOURCE([[
#include <sys/types.h>
#include <sys/mman.h>
int main ()
{
  void *p = mmap (0, 0x10000, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  return (p == (void *)(-1));
}]])],
      [sv_cv_func_mmap_anonymous=yes],
      [sv_cv_func_mmap_anonymous=no],
      [
        dnl FIXME: Put in some more known values here.
        case "$host_os" in
          hpux* | linux* | osf*)
            sv_cv_func_mmap_anonymous=yes ;;
          darwin*)
            sv_cv_func_mmap_anonymous=no ;;
          *)
            AC_LINK_IFELSE([
              AC_LANG_PROGRAM([[
#include <sys/types.h>
#include <sys/mman.h>
]],
                [[mmap (0, 0x10000, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);]])],
              [sv_cv_func_mmap_anonymous="guessing yes"],
              [sv_cv_func_mmap_anonymous=no])
            ;;
        esac
      ])
  ])
  if test "$sv_cv_func_mmap_anonymous" != no; then
    AC_DEFINE([HAVE_MMAP_ANONYMOUS], [1],
      [Define if <sys/mman.h> defines MAP_ANONYMOUS and mmaping with MAP_ANONYMOUS
   works.])
  fi

  dnl 3) MAP_FILE of /dev/zero

  AC_CACHE_CHECK([for mmap of /dev/zero], [sv_cv_func_mmap_devzero], [
    AC_RUN_IFELSE([
      AC_LANG_SOURCE([[
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#ifndef MAP_FILE
#define MAP_FILE 0
#endif
int main ()
{
  int fd;
  void *p;
  fd = open ("/dev/zero", O_RDONLY, 0666);
  if (fd < 0) return 1;
  p = mmap (0, 0x10000, PROT_READ | PROT_WRITE, MAP_FILE | MAP_PRIVATE, fd, 0);
  return (p == (void *)(-1));
}]])],
      [sv_cv_func_mmap_devzero=yes],
      [sv_cv_func_mmap_devzero=no],
      [
        dnl FIXME: Put in some more known values here.
        case "$host_os" in
          freebsd* | irix* | linux* | osf* | solaris* | sunos4*)
            sv_cv_func_mmap_devzero=yes ;;
          darwin*)
            sv_cv_func_mmap_devzero=no ;;
          *)
            AC_LINK_IFELSE([
              AC_LANG_PROGRAM([[
#include <sys/types.h>
#include <sys/mman.h>
#ifndef MAP_FILE
#define MAP_FILE 0
#endif
]], [[mmap (0, 0x10000, PROT_READ | PROT_WRITE, MAP_FILE | MAP_PRIVATE, 7, 0);]])],
              [sv_cv_func_mmap_devzero="guessing yes"],
              [sv_cv_func_mmap_devzero=no])
            ;;
        esac
      ])
  ])
  if test "$sv_cv_func_mmap_devzero" != no; then
    AC_DEFINE([HAVE_MMAP_DEVZERO], [1],
      [Define if mmaping of the special device /dev/zero works.])
  fi
])
