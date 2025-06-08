/* Test the version information.
   Copyright (C) 2025  Bruno Haible <bruno@clisp.org>

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

#include <sigsegv.h>

#include <stdio.h>

int
main ()
{
  if (!(libsigsegv_version == LIBSIGSEGV_VERSION))
    {
      fprintf (stderr, "Installation problem: include file says version 0x%x, whereas library says version 0x%x\n",
               LIBSIGSEGV_VERSION, libsigsegv_version);
      return 1;
    }
  return 0;
}
