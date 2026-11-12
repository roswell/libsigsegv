/* Support functions for Windows on arm64 CPU */

/*
 * Copyright 2026 Bruno Haible <bruno@clisp.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/* Available registers: x9, ..., x17. */
/* Assembler syntax: Windows (clang). */
/* Object file format: COFF. */

/* On Windows/arm64, after our exception handler returns with value
   EXCEPTION_CONTINUE_EXECUTION, the system verifies that the new stack pointer
   (= ExceptionInfo->ContextRecord->Sp) is within the known stack range,
   stored in the TEB.  For an alternate stack, this fails with an exception of
   type "An attempt was made to assign an invalid register context."
   As a workaround, leave ExceptionInfo->ContextRecord->Sp unchanged and, once
   the system's check has passed, jump to this trampoline here, that
     1. sets the stack pointer,
     2. passes the control to desired location.
   It is important that this trampoline consumes no stack space.  */
	.def	setsp_trampoline;  // Define a symbol.
	.scl	2;		   // Set the symbol class to C_EXT (external).
	.type	32;		   // Set the symbol type to DT_FCN (function).
	.endef			   // End the symbol definition.
	.text
	.p2align 2
setsp_trampoline:
	mov	sp, x9		   // Copy %x9 to %sp.
	br	x10		   // Jump to function whose address is in %x10.

/* On 64-bit native Windows, the Microsoft library has setjmp/longjmp
   implementations that unwind the stack, in order to execute C++ destructors.
   <https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/setjmp>
   <https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/longjmp>
   Here, when jumping back from the alternate stack to the main stack, this
   unwinding mechanism gets confused and triggers an exception with code
   0xC0000028 = STATUS_BAD_STACK, which means
   "An invalid or unaligned stack was encountered during an unwind operation."
   The trick that works on Windows/x86_64 does not help on Windows/arm64.
   See <https://lists.gnu.org/archive/html/qemu-devel/2023-02/msg03532.html>.
   Therefore use a longjmp implementation that never does unwinding.
   Taken from mingw-w64, files
     mingw-w64-crt/misc/longjmp.S
     mingw-w64-crt/misc/mingw_longjmp.S
   (public domain).  */
	.def	stackoverflow_longjmp; // Define a symbol.
	.scl	2;		   // Set the symbol class to C_EXT (external).
	.type	32;		   // Set the symbol type to DT_FCN (function).
	.endef			   // End the symbol definition.
	.text
	.p2align 2
stackoverflow_longjmp:
	ldp	x19, x20, [x0, #0x10]	// jmp_buf->X19, X20
	ldp	x21, x22, [x0, #0x20]	// jmp_buf->X21, X22
	ldp	x23, x24, [x0, #0x30]	// jmp_buf->X23, X24
	ldp	x25, x26, [x0, #0x40]	// jmp_buf->X25, X26
	ldp	x27, x28, [x0, #0x50]	// jmp_buf->X27, X28
	ldp	x29, x30, [x0, #0x60]	// jmp_buf->Fp,  Lr
	ldr	x2,       [x0, #0x70]	// jmp_buf->Sp
	mov	sp,  x2
	ldr	w2,       [x0, #0x78]	// jmp_buf->Fpcr
	msr	fpcr, x2
	ldr	w2,       [x0, #0x7c]	// jmp_buf->Fpsr
	msr	fpsr, x2
	ldp	d8,  d9,  [x0, #0x80]	// jmp_buf->D[0-1]
	ldp	d10, d11, [x0, #0x90]	// jmp_buf->D[2-3]
	ldp	d12, d13, [x0, #0xa0]	// jmp_buf->D[4-5]
	ldp	d14, d15, [x0, #0xb0]	// jmp_buf->D[6-7]
	mov	x0,  x1              	// retval
	ret
