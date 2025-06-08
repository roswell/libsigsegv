## Makefile for libsigsegv/install-tests.
## Copyright (C) 2025 Bruno Haible <bruno@clisp.org>
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

installcheck-local:
	$(CC) -I$(includedir) -L$(libdir) \
	      $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) \
	      $(AM_CFLAGS) $(CFLAGS) $(AM_LDFLAGS) $(LDFLAGS) \
	      -o test-version $(srcdir)/test-version.c -lsigsegv
	@AUGMENT_SHLIBPATH@ ./test-version
	rm -rf test-version test-version$(EXEEXT)

EXTRA_DIST = test-version.c
CLEANFILES = test-version test-version$(EXEEXT)
