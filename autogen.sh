#!/bin/sh
# Convenience script for regenerating all autogeneratable files that are
# omitted from the version control repository. In particular, this script
# also regenerates all aclocal.m4, config.h.in, Makefile.in, configure files
# with new versions of autoconf or automake.
#
# This script requires autoconf-2.63..2.68 and automake-1.11 in the PATH.
# It also requires either
#   - the GNULIB_TOOL environment variable pointing to the gnulib-tool script
#     in a gnulib checkout, or
#   - an internet connection.

# Copyright (C) 2009-2010 Free Software Foundation, Inc.
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

# Fetch config.guess, config.sub.
if test -n "$GNULIB_TOOL"; then
  for file in config.guess config.sub; do
    $GNULIB_TOOL --copy-file build-aux/$file; chmod a+x build-aux/$file
  done
else
  for file in config.guess config.sub; do
    wget -q --timeout=5 -O build-aux/$file.tmp "http://git.savannah.gnu.org/gitweb/?p=gnulib.git;a=blob_plain;f=build-aux/${file};hb=HEAD" \
      && mv build-aux/$file.tmp build-aux/$file \
      && chmod a+x build-aux/$file
  done
fi

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
