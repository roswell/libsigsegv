/* Fault handler information subroutine.  NetBSD/Alpha version.
 * Taken from gcc-3.3/boehm-gc/os_dep.c.
 *
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991-1995 by Xerox Corporation.  All rights reserved.
 * Copyright (c) 1996-1999 by Silicon Graphics.  All rights reserved.
 * Copyright (c) 1999 by Hewlett-Packard Company.  All rights reserved.
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to use or copy this program
 * for any purpose,  provided the above notices are retained on all copies.
 * Permission to modify the code and to distribute modified code is granted,
 * provided the above notices are retained, and a notice that the code was
 * modified is included with the above copyright notice.
 */

/* Decodes the machine instruction which was responsible for the sending of the
   SIGBUS signal.  Luckily this is much easier than, say, on the PowerPC.  */

static void *
get_fault_addr (struct sigcontext *scp)
{
  unsigned int instr = *((unsigned int *)(scp->sc_pc));
  unsigned long faultaddr;

  faultaddr = scp->sc_regs[(instr >> 16) & 0x1f];
  faultaddr += (unsigned long) (((int)instr << 16) >> 16);
  return (void *) faultaddr;
}
