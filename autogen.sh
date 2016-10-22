#!/bin/sh
# Convenience script for regenerating all autogeneratable files that are
# omitted from the version control repository. In particular, this script
# also regenerates all aclocal.m4, config.h.in, Makefile.in, configure files
# with new versions of autoconf or automake.
#
# This script requires autoconf-2.63..2.69 and automake-1.11..1.15 in the PATH.
# If the option --skip-gnulib is not given, it also requires either
#   - the GNULIB_TOOL environment variable pointing to the gnulib-tool script
#     in a gnulib checkout, or
#   - an internet connection.

# Copyright (C) 2009-2016 Free Software Foundation, Inc.
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

# Usage: ./autogen.sh [--skip-gnulib]
#
# Usage from a CVS checkout:                 ./autogen.sh
# This uses an up-to-date gnulib checkout.
#
# Usage from a released tarball:             ./autogen.sh --skip-gnulib
# This does not use a gnulib checkout.

skip_gnulib=false
while :; do
  case "$1" in
    --skip-gnulib) skip_gnulib=true; shift;;
    *) break ;;
  esac
done

if test $skip_gnulib = false; then
  if test -z "$GNULIB_TOOL"; then
    # Check out gnulib in a subdirectory 'gnulib'.
    if test -d gnulib; then
      (cd gnulib && git pull)
    else
      git clone git://git.savannah.gnu.org/gnulib.git
    fi
    # Now it should contain a gnulib-tool.
    if test -f gnulib/gnulib-tool; then
      GNULIB_TOOL=`pwd`/gnulib/gnulib-tool
    else
      echo "** warning: gnulib-tool not found" 1>&2
    fi
  fi
  # Skip the gnulib-tool step if gnulib-tool was not found.
  if test -n "$GNULIB_TOOL"; then
    $GNULIB_TOOL --copy-file m4/relocatable-lib.m4
    # Fetch config.guess, config.sub.
    for file in config.guess config.sub; do
      $GNULIB_TOOL --copy-file build-aux/$file; chmod a+x build-aux/$file
    done
  fi
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
