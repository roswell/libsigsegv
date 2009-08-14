#!/bin/sh
# Convenience script for regenerating all autogeneratable files that are
# omitted from the version control repository. In particular, this script
# also regenerates all aclocal.m4, config.h.in, Makefile.in, configure files
# with new versions of autoconf or automake.
#
# This script requires autoconf-2.63 and automake-1.11 in the PATH.

# Copyright (C) 2009 Free Software Foundation, Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# Usage: ./autogen.sh

# Generate aclocal.m4.
aclocal -I m4
# Generate configure.
autoconf
# Generate config.h.in.
autoheader && touch config.h.in
# Generate Makefile.in, src/Makefile.in, tests/Makefile.in, and some
# build-aux/* files.
automake --add-missing --copy

# Generate config.h.msvc, src/sigsegv.h.msvc.
./configure \
  && make config.h.msvc \
  && (cd src && make sigsegv.h.msvc) \
  && make distclean
