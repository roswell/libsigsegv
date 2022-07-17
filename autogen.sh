#!/bin/sh
# Convenience script for regenerating all autogeneratable files that are
# omitted from the version control repository. In particular, this script
# also regenerates all aclocal.m4, config.h.in, Makefile.in, configure files
# with new versions of autoconf or automake.
#
# This script requires autoconf-2.63..2.71 and automake-1.11..1.16.3 in the PATH.

# Copyright (C) 2009-2022 Free Software Foundation, Inc.
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
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

# Prerequisite (if not used from a released tarball): ./autopull.sh
# Usage: ./autogen.sh

# Generate aclocal.m4.
aclocal -I m4
# Generate configure.
autoconf
# Generate config.h.in.
autoheader && touch config.h.in
# Generate Makefile.in, src/Makefile.in, tests/Makefile.in, and some
# build-aux/* files.
# Make sure we get new versions of files brought in by automake.
(cd build-aux && rm -f ar-lib compile depcomp install-sh mdate-sh missing test-driver)
automake --add-missing --copy
# Get rid of autom4te.cache directory.
rm -rf autom4te.cache

echo "$0: done.  Now you can run './configure'."
