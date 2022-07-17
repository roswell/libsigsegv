#!/bin/sh
# Convenience script for fetching auxiliary files that are omitted from
# the version control repository of this package.
#
# This script requires either
#   - the GNULIB_SRCDIR environment variable pointing to a gnulib checkout, or
#   - a preceding invocation of './gitsub.sh pull'.

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

# Usage: ./autopull.sh

./gitsub.sh pull

if test -n "$GNULIB_SRCDIR"; then
  test -d "$GNULIB_SRCDIR" || {
    echo "*** GNULIB_SRCDIR is set but does not point to an existing directory." 1>&2
    exit 1
  }
else
  GNULIB_SRCDIR=`pwd`/gnulib
  test -d "$GNULIB_SRCDIR" || {
    echo "*** Subdirectory 'gnulib' does not yet exist. Use './gitsub.sh pull' to create it, or set the environment variable GNULIB_SRCDIR." 1>&2
    exit 1
  }
fi
# Now it should contain a gnulib-tool.
GNULIB_TOOL="$GNULIB_SRCDIR/gnulib-tool"
test -f "$GNULIB_TOOL" || {
  echo "*** gnulib-tool not found." 1>&2
  exit 1
}
$GNULIB_TOOL --copy-file m4/musl.m4
$GNULIB_TOOL --copy-file m4/relocatable-lib.m4
$GNULIB_TOOL --copy-file m4/sigaltstack.m4
$GNULIB_TOOL --copy-file m4/stack-direction.m4
# Fetch config.guess, config.sub.
for file in config.guess config.sub; do
  $GNULIB_TOOL --copy-file build-aux/$file; chmod a+x build-aux/$file
done

echo "$0: done.  Now you can run './autogen.sh'."
