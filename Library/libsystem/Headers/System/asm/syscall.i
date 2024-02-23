;
;  syscall.i
;  libsystem
;
;  Created by Dietmar Planitzer on 9/6/23.
;  Copyright © 2023 Dietmar Planitzer. All rights reserved.
;

        ifnd __ABI_SYSCALL_I
__ABI_SYSCALL_I  set 1

SC_read                     equ 0
SC_write                    equ 1
SC_delay                    equ 2
SC_dispatch                 equ 3
SC_alloc_address_space      equ 4
SC_exit                     equ 5
SC_spawn_process            equ 6
SC_getpid                   equ 7
SC_getppid                  equ 8
SC_getpargs                 equ 9
SC_open                     equ 10
SC_close                    equ 11
SC_waitpid                  equ 12
SC_seek                     equ 13
SC_getcwd                   equ 14
SC_setcwd                   equ 15
SC_getuid                   equ 16
SC_getumask                 equ 17
SC_setumask                 equ 18
SC_mkdir                    equ 19
SC_getfileinfo              equ 20
SC_opendir                  equ 21
SC_setfileinfo              equ 22
SC_access                   equ 23
SC_fgetfileinfo             equ 24
SC_fsetfileinfo             equ 25
SC_unlink                   equ 26
SC_rename                   equ 27
SC_ioctl                    equ 28
SC_truncate                 equ 29
SC_ftruncate                equ 30
SC_mkfile                   equ 31
SC_mkpipe                   equ 32
SC_dispatch_after           equ 33
SC_dispatch_queue_create    equ 34
SC_dispatch_queue_current   equ 35
SC_dispose                  equ 36
SC_SC_get_monotonic_time    equ 37

SC_numberOfCalls            equ 38


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

        endif   ; __ABI_SYSCALL_I
