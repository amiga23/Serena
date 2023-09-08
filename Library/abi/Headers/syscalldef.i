;
;  syscalldef.i
;  Apollo
;
;  Created by Dietmar Planitzer on 9/6/23.
;  Copyright © 2023 Dietmar Planitzer. All rights reserved.
;

        ifnd _SYSCALLDEF_I
_SYSCALLDEF_I  set 1

SC_write                equ 0   ; ssize_t write(const char *buffer, size_t count)
SC_sleep                equ 1   ; errno_t sleep(struct {int secs, int nanosecs})
SC_dispatch_async       equ 2   ; errno_t dispatch_async(void *pUserClosure)
SC_alloc_address_space  equ 3   ; errno_t alloc_address_space(int nbytes, void **pOutMem)
SC_exit                 equ 4   ; _Noreturn exit(int status)
SC_spawn_process        equ 5   ; errno_t spawn_process(void *pUserEntryPoint)

SC_numberOfCalls        equ 6   ; number of system calls


; System call macro.
;
; A system call looks like this:
;
; a0.l: -> pointer to argument list base
;
; d0.l: <- error number
;
; Register a0 holds a pointer to the base of the argument list. Arguments are
; expected to be ordered from left to right (same as the standard C function
; call ABI) and the pointer in a0 points to the left-most argument. So the
; simplest way to pass arguments to a system call is to push them on the user
; stack starting with the right-most argument and ending with the left-most
; argument and to then initialize a0 like this:
;
; move.l sp, a0
;
; If the arguments are first put on the stack and you then call a subroutine
; which does the actual trap #0 to the kernel, then you want to initialize a0
; like this:
;
; lea 4(sp), a0
;
; since the user stack pointer points to the return address on the stack and not
; the system call number.
;
; The system call returns the error code in d0.
;
    macro SYSCALL
    trap    #0
    endm

        endif   ; _SYSCALLDEF_I