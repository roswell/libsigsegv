/* Determine the virtual memory area of a given address.  Win32 version.
   Copyright (C) 2002, 2016  Bruno Haible <bruno@clisp.org>

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

#include "stackvma.h"
#include <windows.h>
#include <stdio.h>

static void DumpProcessMemoryMap()
{
  MEMORY_BASIC_INFORMATION info;
  uintptr_t address = 0;
  printf("Memory dump:\n");
  while (VirtualQuery((void*)address,&info,sizeof(info)) == sizeof(info))
    {
      /* Always info.BaseAddress = address.  */
      switch (info.State)
        {
        case MEM_FREE:    printf("-"); break;
        case MEM_RESERVE: printf("+"); break;
        case MEM_COMMIT:  printf("*"); break;
        default: printf("?"); break;
        }
      printf(" 0x%lx - 0x%lx",
             (uintptr_t)info.BaseAddress,
             (uintptr_t)info.BaseAddress+info.RegionSize-1);
      if (info.State != MEM_FREE)
        {
          printf(" (0x%lx) ",(uintptr_t)info.AllocationBase);
          /* info.AllocationProtect is apparently irrelevant.  */
          switch (info.Protect & ~(PAGE_GUARD|PAGE_NOCACHE))
            {
            case PAGE_READONLY:          printf(" R  "); break;
            case PAGE_READWRITE:         printf(" RW "); break;
            case PAGE_WRITECOPY:         printf(" RWC"); break;
            case PAGE_EXECUTE:           printf("X   "); break;
            case PAGE_EXECUTE_READ:      printf("XR  "); break;
            case PAGE_EXECUTE_READWRITE: printf("XRW "); break;
            case PAGE_EXECUTE_WRITECOPY: printf("XRWC"); break;
            case PAGE_NOACCESS:          printf("----"); break;
            default: printf("?"); break;
            }
          if (info.Protect & PAGE_GUARD)
            printf(" PAGE_GUARD");
          if (info.Protect & PAGE_NOCACHE)
            printf(" PAGE_NOCACHE");
          printf(" ");
          switch (info.Type)
            {
            case MEM_IMAGE:   printf("MEM_IMAGE"); break;
            case MEM_MAPPED:  printf("MEM_MAPPED"); break;
            case MEM_PRIVATE: printf("MEM_PRIVATE"); break;
            default:          printf("MEM_?"); break;
            }
        }
      printf("\n");
      address = (uintptr_t)info.BaseAddress + info.RegionSize;
    }
  printf("End of memory dump.\n");
}

int
sigsegv_get_vma (uintptr_t address, struct vma_struct *vma)
{
  DumpProcessMemoryMap();
  return -1;
}
