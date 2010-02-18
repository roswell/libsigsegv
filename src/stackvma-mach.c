/* Determine the virtual memory area of a given address.  Mach version.
   Copyright (C) 2003, 2006  Paolo Bonzini <bonzini@gnu.org>
   Copyright (C) 2010  Bruno Haible <bruno@clisp.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

#include "stackvma.h"
#include <stdio.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <libc.h>
#include <nlist.h>
#include <mach/mach.h>
#ifndef NeXT
#include <mach/machine/vm_param.h>
#endif

#include "stackvma-simple.c"

int
sigsegv_get_vma (unsigned long req_address, struct vma_struct *vma)
{
  unsigned long prev_address = 0, prev_size = 0;
  unsigned long join_address = 0, join_size = 0;
  int more = 1;
  vm_address_t address;
  vm_size_t size;
#ifdef VM_REGION_BASIC_INFO
  /* MacOS X API */
  task_t task = mach_task_self ();
#else
  task_t task = task_self ();
  vm_prot_t protection, max_protection;
  vm_inherit_t inheritance;
  boolean_t shared;
  vm_offset_t offset;
#endif

  for (address = VM_MIN_ADDRESS; more; address += size)
    {
      mach_port_t object_name;
#ifdef VM_REGION_BASIC_INFO
      /* MacOS X API */
      /* In MacOS X 10.5, the types vm_address_t, vm_offset_t, vm_size_t have
         32 bits in 32-bit processes and 64 bits in 64-bit processes. Whereas
         mach_vm_address_t and mach_vm_size_t are always 64 bits large.
         MacOS X 10.5 has three vm_region like methods:
           - vm_region. It has arguments that depend on whether the current
             process is 32-bit or 64-bit. When linking dynamically, this
             function exists only in 32-bit processes. Therefore we use it only
             in 32-bit processes.
           - vm_region_64. It has arguments that depend on whether the current
             process is 32-bit or 64-bit. It interprets a flavor
             VM_REGION_BASIC_INFO as VM_REGION_BASIC_INFO_64, which is
             dangerous since 'struct vm_region_basic_info_64' is larger than
             'struct vm_region_basic_info'; therefore let's write
             VM_REGION_BASIC_INFO_64 explicitly.
           - mach_vm_region. It has arguments that are 64-bit always. This
             function is useful when you want to access the VM of a process
             other than the current process.
         In 64-bit processes, we could use vm_region_64 or mach_vm_region.
         I choose vm_region_64 because it uses the same types as vm_region,
         resulting in less conditional code.  */
# if defined __ppc64__ || defined __x86_64__
      struct vm_region_basic_info_64 info;
      mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;

      more = (vm_region_64 (task, &address, &size, VM_REGION_BASIC_INFO_64,
                            (vm_region_info_t)&info, &info_count, &object_name)
              == KERN_SUCCESS);
# else
      struct vm_region_basic_info info;
      mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT;

      more = (vm_region (task, &address, &size, VM_REGION_BASIC_INFO,
                         (vm_region_info_t)&info, &info_count, &object_name)
              == KERN_SUCCESS);
# endif
#else
      more = (vm_region (task, &address, &size, &protection, &max_protection,
                         &inheritance, &shared, &object_name, &offset)
              == KERN_SUCCESS);
#endif
      if (!more)
        {
          address = join_address + join_size;
          size = 0;
        }

      if ((unsigned long) address == join_address + join_size)
        join_size += size;
      else
        {
          prev_address = join_address;
          prev_size = join_size;
          join_address = (unsigned long) address;
          join_size = size;
        }

#ifdef VM_REGION_BASIC_INFO
      if (object_name != MACH_PORT_NULL)
        mach_port_deallocate (mach_task_self (), object_name);
#endif

#if STACK_DIRECTION < 0
      if (join_address <= req_address && join_address + join_size > req_address)
        {
          vma->start = join_address;
          vma->end = join_address + join_size;
          vma->prev_end = prev_address + prev_size;
          vma->is_near_this = simple_is_near_this;
          return 0;
        }
#else
      if (prev_address <= req_address && prev_address + prev_size > req_address)
        {
          vma->start = prev_address;
          vma->end = prev_address + prev_size;
          vma->next_start = join_address;
          vma->is_near_this = simple_is_near_this;
          return 0;
        }
#endif
    }

#if STACK_DIRECTION > 0
  if (join_address <= req_address && join_address + size > req_address)
    {
      vma->start = prev_address;
      vma->end = prev_address + prev_size;
      vma->next_start = ~0UL;
      vma->is_near_this = simple_is_near_this;
      return 0;
    }
#endif

  return -1;
}
