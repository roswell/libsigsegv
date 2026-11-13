/* Fault handler information.  Woe32 version.
   Copyright (C) 1993-1999, 2002-2003, 2007-2009, 2011, 2016, 2026  Bruno Haible <bruno@clisp.org>
   Copyright (C) 2003  Paolo Bonzini <bonzini@gnu.org>
   Copyright (C) 2009  Eric Blake <ebb9@byu.net>

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

#include "sigsegv.h"

/* Whether to enable debugging output.  */
#define DEBUG_HANDLER       0
/* Whether to enable debugging output in the main_exception_filter function.
   In 32-bit mode, this can often make the main_exception_filter function
   crash.  Therefore, enable it only in 64-bit mode.  */
#define DEBUG_MAIN_HANDLER  (DEBUG_HANDLER && defined _WIN64)

#define WIN32_LEAN_AND_MEAN /* avoid including junk */
#include <windows.h>
#include <winerror.h>
/*
 * extern LPTOP_LEVEL_EXCEPTION_FILTER SetUnhandledExceptionFilter (LPTOP_LEVEL_EXCEPTION_FILTER TopLevelExceptionFilter);
 * extern PVOID AddVectoredExceptionHandler (DWORD First, PVECTORED_EXCEPTION_HANDLER VectoredHandler);
 * extern DWORD VirtualQuery (LPCVOID Address, PMEMORY_BASIC_INFORMATION Buffer, DWORD Length);
 * extern BOOL VirtualProtect (LPVOID Address, DWORD Size, DWORD NewProtect, PDWORD OldProtect);
 * extern DWORD GetLastError (void);
 */

#if DEBUG_HANDLER
# include <stdio.h>
#endif

/* On Cygwin, the "system calls" are actually library calls.  If the user
   asks for them to return with errno = EFAULT, we do this by combining
   the Win32 and the Unix approaches:
     1. Install a Win32 exception filter and use it to peek at the fault
        context, but without actually handling the fault.
     2. Install a Unix signal handler.
   If a fault occurs outside a "system call", our exception filter will
   take a note and defer, Cygwin's exception filter will invoke our Unix
   signal handler.
   If a fault occurs inside a "system call", our exception filter will
   take a note and defer, Cygwin's exception filter will handle it, and
   our signal handler will not be called.
   For stack overflow, we use the pure Win32 approach, since Cygwin does
   not have sigaltstack().  */
#if defined __CYGWIN__ && ENABLE_EFAULT

# define MIXING_UNIX_SIGSEGV_AND_WIN32_STACKOVERFLOW_HANDLING 1

# if OLD_CYGWIN_WORKAROUND
/* Last seen fault address.  */
static void *last_seen_fault_address;
# endif

/* Import the relevant part of the Unix approach.  */
# undef HAVE_STACK_OVERFLOW_RECOVERY
# define HAVE_STACK_OVERFLOW_RECOVERY 0
# define sigsegv_install_handler sigsegv_install_handler_unix
# include "handler-unix.c"
# undef sigsegv_install_handler

#else

/* User's SIGSEGV handler.  */
static sigsegv_handler_t user_handler = (sigsegv_handler_t) NULL;

#endif

/* Stack overflow handling is tricky:
   First, we must catch a EXCEPTION_STACK_OVERFLOW exception. This is signalled
   when the guard page at the end of the stack has been touched. The operating
   system remaps the page with protection PAGE_READWRITE and only then calls
   our exception handler. Actually, it's even more complicated: The stack has
   the following layout:

           |                             |guard|----------stack-----------|

   and when the guard page is touched, the system maps it PAGE_READWRITE and
   allocates a new guard page below it:

           |                       |guard|-------------stack--------------|

   Only when no new guard page can be allocated (because the maximum stack
   size has been reached), will we see an exception.

           |guard|-------------------------stack--------------------------|

   Second, we must reinstall the guard page. Otherwise, on the next stack
   overflow, the application will simply crash (on WinNT: silently, on Win95:
   with an error message box and freezing the system).
   But since we don't know where %esp points to during the exception handling,
   we must first leave the exception handler, before we can restore the guard
   page. And %esp must be made to point to a reasonable value before we do
   this.

   See also
   <https://devblogs.microsoft.com/oldnewthing/20220203-00/?p=106215/>.

   Note: On WinNT, the guard page has protection PAGE_READWRITE|PAGE_GUARD.
   On Win95, which doesn't know PAGE_GUARD, it has protection PAGE_NOACCESS.
 */

static stackoverflow_handler_t stk_user_handler =
  (stackoverflow_handler_t) NULL;
static uintptr_t stk_extra_stack;
static size_t stk_extra_stack_size;

static void
stack_overflow_handler (uintptr_t faulting_page_address, stackoverflow_context_t context)
{
#if DEBUG_HANDLER
  puts ("In stack_overflow_handler"); fflush (stdout);
#endif

  /* The Microsoft library has a function _resetstkoflw() for reinstalling the
     guard page.
     <https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/resetstkoflw>
     But we cannot use it here, because it does not take a parameter.
     By design, it works only after the longjmp() back into the normal thread
     has been executed.  */

  /* First get stack's base address.  */
  MEMORY_BASIC_INFORMATION info;
  if (VirtualQuery ((void*) faulting_page_address, &info, sizeof (info))
      != sizeof (info))
    goto failed;
  uintptr_t base = (uintptr_t) info.AllocationBase;

  /* Now search for the first existing page.  */
  uintptr_t address;
  {
    address = base;
    for (;;)
      {
        if (VirtualQuery ((void*) address, &info, sizeof (info)) != sizeof (info))
          goto failed;
        if (address != (uintptr_t) info.BaseAddress)
          goto failed;
        if (info.State != MEM_FREE)
          {
            if ((uintptr_t) info.AllocationBase != base)
              goto failed;
            if (info.State == MEM_COMMIT)
              break;
          }
        address = (uintptr_t) info.BaseAddress + info.RegionSize;
      }
  }

  /* We can put the new guard page on the first existing page.  It would also be
     possible to put it at the second or third existing page, for instance.
     In fact, in some 64-bit configurations (especially when DEBUG_HANDLER is 1)
     the first existing page does not work; the third one works much better.  */
  uintptr_t guard_page = address + 2 * 0x1000;

  /* TODO: Consider the minimum thread stack size.
     <https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setthreadstackguarantee>
   */

  /* Now add the PAGE_GUARD bit to the designated page.  */
#if 0
  /* _resetstkoflw() does this.  But it is not needed.  */
  if (VirtualAlloc ((void *) guard_page, 0x1000, MEM_COMMIT, info.Protect) == NULL)
    goto failed;
#endif
  /* On WinNT this works...  */
  DWORD oldprot;
  if (VirtualProtect ((void *) guard_page, 0x1000, info.Protect | PAGE_GUARD, &oldprot))
    goto ok;
  if (GetLastError () == ERROR_INVALID_PARAMETER)
    /* ... but on Win95 we need this:  */
    if (VirtualProtect ((void *) guard_page, 0x1000, PAGE_NOACCESS, &oldprot))
      goto ok;
 failed:
#if DEBUG_HANDLER
  puts ("In stack_overflow_handler: failed."); fflush (stdout);
#endif
  for (;;)
    (*stk_user_handler) (1, context);
 ok:
#if DEBUG_HANDLER
  puts ("In stack_overflow_handler: ok."); fflush (stdout);
#endif
  for (;;)
    (*stk_user_handler) (0, context);
}

#if defined _M_ARM64
/* Defined in assembly language, in a separate compilation unit (because
   MSVC on arm64 does not support inline asm).  */
extern void setsp_trampoline (void);
#endif

/* This is the stack overflow and page fault handler.  */
static LONG WINAPI
main_exception_filter (EXCEPTION_POINTERS *ExceptionInfo)
{
#if DEBUG_MAIN_HANDLER
  printf ("In main_exception_filter: Code = 0x%x, NumberParameters = %u\n",
          ExceptionInfo->ExceptionRecord->ExceptionCode,
          ExceptionInfo->ExceptionRecord->NumberParameters);
  fflush (stdout);
#endif
  if ((stk_user_handler
       && ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_STACK_OVERFLOW
      )
#if !MIXING_UNIX_SIGSEGV_AND_WIN32_STACKOVERFLOW_HANDLING || OLD_CYGWIN_WORKAROUND
      ||
      (ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION
#if !MIXING_UNIX_SIGSEGV_AND_WIN32_STACKOVERFLOW_HANDLING
       && user_handler != (sigsegv_handler_t)NULL
#endif
      )
#endif
     )
    {
#if DEBUG_MAIN_HANDLER
      printf ("Exception!\n");
      printf ("Code = 0x%x\n",
              ExceptionInfo->ExceptionRecord->ExceptionCode);
      printf ("Flags = 0x%x\n",
              ExceptionInfo->ExceptionRecord->ExceptionFlags);
      printf ("Address = 0x%x\n",
              ExceptionInfo->ExceptionRecord->ExceptionAddress);
      printf ("Params:");
      {
        DWORD i;
        for (i = 0; i < ExceptionInfo->ExceptionRecord->NumberParameters; i++)
          printf (" 0x%x,",
                  ExceptionInfo->ExceptionRecord->ExceptionInformation[i]);
      }
      printf ("\n");
      printf ("Registers:\n");
# if defined _M_IX86
      printf ("eip = 0x%x\n", ExceptionInfo->ContextRecord->Eip);
      printf ("eax = 0x%x, ", ExceptionInfo->ContextRecord->Eax);
      printf ("ebx = 0x%x, ", ExceptionInfo->ContextRecord->Ebx);
      printf ("ecx = 0x%x, ", ExceptionInfo->ContextRecord->Ecx);
      printf ("edx = 0x%x\n", ExceptionInfo->ContextRecord->Edx);
      printf ("esi = 0x%x, ", ExceptionInfo->ContextRecord->Esi);
      printf ("edi = 0x%x, ", ExceptionInfo->ContextRecord->Edi);
      printf ("ebp = 0x%x, ", ExceptionInfo->ContextRecord->Ebp);
      printf ("esp = 0x%x\n", ExceptionInfo->ContextRecord->Esp);
# endif
# if defined _M_X64
      printf ("rip = 0x%llx\n", ExceptionInfo->ContextRecord->Rip);
      printf ("rax = 0x%llx, ", ExceptionInfo->ContextRecord->Rax);
      printf ("rbx = 0x%llx, ", ExceptionInfo->ContextRecord->Rbx);
      printf ("rcx = 0x%llx, ", ExceptionInfo->ContextRecord->Rcx);
      printf ("rdx = 0x%llx\n", ExceptionInfo->ContextRecord->Rdx);
      printf ("rsi = 0x%llx, ", ExceptionInfo->ContextRecord->Rsi);
      printf ("rdi = 0x%llx, ", ExceptionInfo->ContextRecord->Rdi);
      printf ("rbp = 0x%llx, ", ExceptionInfo->ContextRecord->Rbp);
      printf ("rsp = 0x%llx\n", ExceptionInfo->ContextRecord->Rsp);
      printf ("r8  = 0x%llx, ", ExceptionInfo->ContextRecord->R8);
      printf ("r9  = 0x%llx, ", ExceptionInfo->ContextRecord->R9);
      printf ("r10 = 0x%llx, ", ExceptionInfo->ContextRecord->R10);
      printf ("r11 = 0x%llx\n", ExceptionInfo->ContextRecord->R11);
      printf ("r12 = 0x%llx, ", ExceptionInfo->ContextRecord->R12);
      printf ("r13 = 0x%llx, ", ExceptionInfo->ContextRecord->R13);
      printf ("r14 = 0x%llx, ", ExceptionInfo->ContextRecord->R14);
      printf ("r15 = 0x%llx\n", ExceptionInfo->ContextRecord->R15);
# endif
# if defined _M_ARM64
      printf ("pc  = 0x%llx, ", ExceptionInfo->ContextRecord->Pc);
      printf ("sp  = 0x%llx, ", ExceptionInfo->ContextRecord->Sp);
      printf ("fp  = 0x%llx, ", ExceptionInfo->ContextRecord->Fp);
      printf ("lr  = 0x%llx\n", ExceptionInfo->ContextRecord->Lr);
      printf ("x0  = 0x%llx, ", ExceptionInfo->ContextRecord->X0);
      printf ("x1  = 0x%llx, ", ExceptionInfo->ContextRecord->X1);
      printf ("x2  = 0x%llx, ", ExceptionInfo->ContextRecord->X2);
      printf ("x3  = 0x%llx\n", ExceptionInfo->ContextRecord->X3);
      printf ("x4  = 0x%llx, ", ExceptionInfo->ContextRecord->X4);
      printf ("x5  = 0x%llx, ", ExceptionInfo->ContextRecord->X5);
      printf ("x6  = 0x%llx, ", ExceptionInfo->ContextRecord->X6);
      printf ("x7  = 0x%llx\n", ExceptionInfo->ContextRecord->X7);
      printf ("x8  = 0x%llx, ", ExceptionInfo->ContextRecord->X8);
      printf ("x9  = 0x%llx, ", ExceptionInfo->ContextRecord->X9);
      printf ("x10 = 0x%llx, ", ExceptionInfo->ContextRecord->X10);
      printf ("x11 = 0x%llx\n", ExceptionInfo->ContextRecord->X11);
      printf ("x12 = 0x%llx, ", ExceptionInfo->ContextRecord->X12);
      printf ("x13 = 0x%llx, ", ExceptionInfo->ContextRecord->X13);
      printf ("x14 = 0x%llx, ", ExceptionInfo->ContextRecord->X14);
      printf ("x15 = 0x%llx\n", ExceptionInfo->ContextRecord->X15);
      printf ("x16 = 0x%llx, ", ExceptionInfo->ContextRecord->X16);
      printf ("x17 = 0x%llx, ", ExceptionInfo->ContextRecord->X17);
      printf ("x18 = 0x%llx, ", ExceptionInfo->ContextRecord->X18);
      printf ("x19 = 0x%llx\n", ExceptionInfo->ContextRecord->X19);
      printf ("x20 = 0x%llx, ", ExceptionInfo->ContextRecord->X20);
      printf ("x21 = 0x%llx, ", ExceptionInfo->ContextRecord->X21);
      printf ("x22 = 0x%llx, ", ExceptionInfo->ContextRecord->X22);
      printf ("x23 = 0x%llx\n", ExceptionInfo->ContextRecord->X23);
      printf ("x24 = 0x%llx, ", ExceptionInfo->ContextRecord->X24);
      printf ("x25 = 0x%llx, ", ExceptionInfo->ContextRecord->X25);
      printf ("x26 = 0x%llx, ", ExceptionInfo->ContextRecord->X26);
      printf ("x27 = 0x%llx\n", ExceptionInfo->ContextRecord->X27);
      printf ("x28 = 0x%llx\n", ExceptionInfo->ContextRecord->X28);
# endif
      fflush (stdout);
#endif
      if (ExceptionInfo->ExceptionRecord->NumberParameters == 2)
        {
          if (stk_user_handler
              && ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_STACK_OVERFLOW)
            {
              char *address = (char *) ExceptionInfo->ExceptionRecord->ExceptionInformation[1];
              /* Restart the program, giving it a sane value for the stack pointer.
                 At the same time, copy the contents of
                 ExceptionInfo->ContextRecord (which, on Windows XP, happens
                 to be allocated in the guard page, where it will be
                 inaccessible as soon as we restore the PAGE_GUARD bit!) to
                 this new stack.  */
              uintptr_t faulting_page_address = (uintptr_t)address & -0x1000;
              uintptr_t new_safe_sp = ((stk_extra_stack + stk_extra_stack_size) & -16);
              CONTEXT *orig_context = ExceptionInfo->ContextRecord;
              CONTEXT *safe_context = (CONTEXT *) (new_safe_sp -= sizeof (CONTEXT)); /* make room */
              memcpy (safe_context, orig_context, sizeof (CONTEXT));
#if defined _M_IX86
              new_safe_sp -= 2 * sizeof (void *); /* make room for arguments */
#endif
              new_safe_sp &= -16; /* align */
#if !defined _M_ARM64 /* On arm64, the return address is passed in register lr, not on the stack.  */
              new_safe_sp -= sizeof (void *); /* make room for (unused) return address slot */
#endif
#if defined _M_IX86
              ExceptionInfo->ContextRecord->Esp = new_safe_sp;
#endif
#if defined _M_X64
              ExceptionInfo->ContextRecord->Rsp = new_safe_sp;
#endif
              /* Call stack_overflow_handler(faulting_page_address,safe_context).  */
#if defined _M_IX86
              ExceptionInfo->ContextRecord->Eip = (uintptr_t)&stack_overflow_handler;
              *(uintptr_t *)(new_safe_sp + 4) = faulting_page_address;
              *(uintptr_t *)(new_safe_sp + 8) = (uintptr_t) safe_context;
#endif
#if defined _M_X64
              ExceptionInfo->ContextRecord->Rip = (uintptr_t)&stack_overflow_handler;
              ExceptionInfo->ContextRecord->Rcx = faulting_page_address;
              ExceptionInfo->ContextRecord->Rdx = (uintptr_t) safe_context;
#endif
#if defined _M_ARM64
              ExceptionInfo->ContextRecord->Pc = (uintptr_t)&setsp_trampoline;
              ExceptionInfo->ContextRecord->X9 = new_safe_sp;
              ExceptionInfo->ContextRecord->X10 = (uintptr_t)&stack_overflow_handler;
              ExceptionInfo->ContextRecord->X0 = faulting_page_address;
              ExceptionInfo->ContextRecord->X1 = (uintptr_t) safe_context;
#endif
#if DEBUG_MAIN_HANDLER
              puts ("In main_exception_filter: returning CONTINUE_EXECUTION."); fflush (stdout);
#endif
              return EXCEPTION_CONTINUE_EXECUTION;
            }
#if !MIXING_UNIX_SIGSEGV_AND_WIN32_STACKOVERFLOW_HANDLING || OLD_CYGWIN_WORKAROUND
          if (ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION)
            {
#if MIXING_UNIX_SIGSEGV_AND_WIN32_STACKOVERFLOW_HANDLING
              /* Store the fault address.  Then pass control to Cygwin's
                 exception filter, which will decide whether to recognize
                 a fault inside a "system call" (and return errno = EFAULT)
                 or to pass it on to the program (by invoking the Unix signal
                 handler).  */
              last_seen_fault_address = (void *) ExceptionInfo->ExceptionRecord->ExceptionInformation[1];
#else
              if (user_handler != (sigsegv_handler_t) NULL)
                {
                  /* ExceptionInfo->ExceptionRecord->ExceptionInformation[0] is
                     1 if it's a write access, 0 if it's a read access. But we
                     don't need this info because we don't have it on Unix
                     either.  */
                  void *address = (void *) ExceptionInfo->ExceptionRecord->ExceptionInformation[1];
                  if ((*user_handler) (address, 1))
                    return EXCEPTION_CONTINUE_EXECUTION;
                }
#endif
            }
#endif
        }
    }
  return EXCEPTION_CONTINUE_SEARCH;
}

#if defined __CYGWIN__ && defined __i386__

/* In Cygwin programs, SetUnhandledExceptionFilter has no effect because Cygwin
   installs a global exception handler.  We have to dig deep in order to install
   our main_exception_filter.  */

/* Data structures for the current thread's exception handler chain.
   On the x86 Windows uses register fs, offset 0 to point to the current
   exception handler; Cygwin mucks with it, so we must do the same... :-/ */

/* Magic taken from winsup/cygwin/include/exceptions.h.  */

struct exception_list
  {
    struct exception_list *prev;
    int (*handler) (EXCEPTION_RECORD *, void *, CONTEXT *, void *);
  };
typedef struct exception_list exception_list;

/* Magic taken from winsup/cygwin/exceptions.cc.  */

__asm__ (".equ __except_list,0");

extern exception_list *_except_list __asm__ ("%fs:__except_list");

/* For debugging.  _except_list is not otherwise accessible from gdb.  */
static
#if __GNUC__ >= 3
__attribute__ ((__unused__)) /* avoid GCC warning */
#endif
exception_list *
debug_get_except_list ()
{
  return _except_list;
}

/* Cygwin's original exception handler.  */
static int (*cygwin_exception_handler) (EXCEPTION_RECORD *, void *, CONTEXT *, void *);

/* Our exception handler.  */
static int
libsigsegv_exception_handler (EXCEPTION_RECORD *exception, void *frame, CONTEXT *context, void *dispatch)
{
  EXCEPTION_POINTERS ExceptionInfo;
  ExceptionInfo.ExceptionRecord = exception;
  ExceptionInfo.ContextRecord = context;
  if (main_exception_filter (&ExceptionInfo) == EXCEPTION_CONTINUE_SEARCH)
    return cygwin_exception_handler (exception, frame, context, dispatch);
  else
    return 0;
}

static void
do_install_main_exception_filter ()
{
  /* We cannot insert any handler into the chain, because such handlers
     must lie on the stack (?).  Instead, we have to replace(!) Cygwin's
     global exception handler.  */
  cygwin_exception_handler = _except_list->handler;
  _except_list->handler = libsigsegv_exception_handler;
}

#else

static void
do_install_main_exception_filter ()
{
# ifdef _WIN64
  /* Should we use SetUnhandledExceptionFilter or AddVectoredExceptionHandler?
     For SIGSEGV handling, either of these works.  For catching stack overflow,
     SetUnhandledExceptionFilter works the first time, but not also the second
     time.  Whereas AddVectoredExceptionHandler works repeatedly.
     The following comment from the Cygwin sources explains it:
       "If another exception occurs while running a signal handler on an
        alternate signal stack, the normal SEH handlers are skipped, because the
        OS exception handling considers the current (alternate) stack "broken".
        However, it still calls vectored exception handlers."
     If we were to use both SetUnhandledExceptionFilter and
     AddVectoredExceptionHandler, only the handler passed to
     AddVectoredExceptionHandler would get used.  */
  AddVectoredExceptionHandler (1, (LPTOP_LEVEL_EXCEPTION_FILTER) &main_exception_filter);
# else
  SetUnhandledExceptionFilter ((LPTOP_LEVEL_EXCEPTION_FILTER) &main_exception_filter);
# endif
}

#endif

static void
install_main_exception_filter ()
{
  static int main_exception_filter_installed = 0;

  if (!main_exception_filter_installed)
    {
      do_install_main_exception_filter ();
      main_exception_filter_installed = 1;
    }
}

#if MIXING_UNIX_SIGSEGV_AND_WIN32_STACKOVERFLOW_HANDLING

int
sigsegv_install_handler (sigsegv_handler_t handler)
{
  install_main_exception_filter ();
  return sigsegv_install_handler_unix (handler);
}

#else

int
sigsegv_install_handler (sigsegv_handler_t handler)
{
  user_handler = handler;
  install_main_exception_filter ();
  return 0;
}

void
sigsegv_deinstall_handler (void)
{
  user_handler = (sigsegv_handler_t) NULL;
}

int
sigsegv_leave_handler (void (*continuation) (void*, void*, void*),
                       void* cont_arg1, void* cont_arg2, void* cont_arg3)
{
  (*continuation) (cont_arg1, cont_arg2, cont_arg3);
  return 1;
}

#endif /* !MIXING_UNIX_SIGSEGV_AND_WIN32_STACKOVERFLOW_HANDLING */

int
stackoverflow_install_handler (stackoverflow_handler_t handler,
                               void *extra_stack, size_t extra_stack_size)
{
  stk_user_handler = handler;
  stk_extra_stack = (uintptr_t) extra_stack;
  stk_extra_stack_size = extra_stack_size;
  install_main_exception_filter ();
  return 0;
}

void
stackoverflow_deinstall_handler (void)
{
  stk_user_handler = (stackoverflow_handler_t) NULL;
}
