## Makefile for libsigsegv/tests.
## Copyright (C) 2002-2003, 2008-2010, 2016, 2021 Bruno Haible <bruno@clisp.org>
##
## This program is free software: you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program.  If not, see <https://www.gnu.org/licenses/>.

## Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.11 gnits no-dependencies color-tests

TESTS = \
  test-catch-segv1 \
  test-catch-segv2 \
  test-segv-dispatcher1 \
  test-catch-stackoverflow1 \
  test-catch-stackoverflow2

EXTRA_DIST = mmap-anon-util.h altstack-util.h

AM_CPPFLAGS = -I../src
DEFS = @DEFS@
LDADD = ../src/libsigsegv.la
noinst_PROGRAMS = \
  test-catch-segv1 \
  test-catch-segv2 \
  test-segv-dispatcher1 \
  test-catch-stackoverflow1 \
  test-catch-stackoverflow2

if CYGWIN
TESTS += cygwin1
noinst_PROGRAMS += cygwin1
endif

if ENABLE_EFAULT
TESTS += efault1 efault2 efault3
noinst_PROGRAMS += efault1 efault2 efault3
endif

# The following rule is necessary to avoid a toplevel "make -n" failure.
../src/libsigsegv.la :
	cd ../src && $(MAKE) libsigsegv.la
