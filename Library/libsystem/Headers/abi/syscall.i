;
;  syscall.i
;  libsystem
;
;  Created by Dietmar Planitzer on 9/6/23.
;  Copyright © 2023 Dietmar Planitzer. All rights reserved.
;

        ifnd __ABI_SYSCALL_I
__ABI_SYSCALL_I  set 1

SC_read                 equ 0   ; errno_t read(int fd, const char * _Nonnull buffer, size_t nBytesToRead, ssize_t* pOutBytesRead)
SC_write                equ 1   ; errno_t write(int fd, const char * _Nonnull buffer, size_t nBytesToWrite, ssize_t* pOutBytesWritten)
SC_sleep                equ 2   ; errno_t sleep(const struct timespec * _Nonnull delay)
SC_dispatch_async       equ 3   ; errno_t dispatch_async(void * _Nonnull pUserClosure)
SC_alloc_address_space  equ 4   ; errno_t alloc_address_space(int nbytes, void **pOutMem)
SC_exit                 equ 5   ; _Noreturn exit(int status)
SC_spawn_process        equ 6   ; errno_t spawnp(struct __spawn_arguments_t * _Nonnull args, pid_t * _Nullable rpid)
SC_getpid               equ 7   ; int getpid(void)
SC_getppid              equ 8   ; int getppid(void)
SC_getpargs             equ 9   ; struct __process_arguments_t* _Nonnull getpargs(void)
SC_open                 equ 10  ; errno_t open(const char * _Nonnull name, int options, int* _Nonnull fd)
SC_close                equ 11  ; errno_t close(int fd)
SC_waitpid              equ 12  ; errno_t waitpid(pid_t pid, struct __waitpid_result_t * _Nullable result)
SC_seek                 equ 13  ; errno_t seek(int fd, off_t offset, off_t *newpos, int whence)
SC_getcwd               equ 14  ; errno_t getcwd(char* buffer, size_t bufferSize)
SC_setcwd               equ 15  ; errno_t setcwd(const char* path)
SC_getuid               equ 16  ; uid_t getuid(void)
SC_getumask             equ 17  ; mode_t getumask(void)
SC_setumask             equ 18  ; void setumask(mode_t mask)
SC_mkdir                equ 19  ; errno_t mkdir(const char* path, mode_t mode)
SC_getfileinfo          equ 20  ; errno_t getfileinfo(const char* path, struct _file_info_t* info)
SC_opendir              equ 21  ; errno_t opendir(const char* path, int* fd)
SC_setfileinfo          equ 22  ; errno_t setfileinfo(const char* path, struct _mutable_file_info_t* info)
SC_access               equ 23  ; errno_t access(const char* path, int mode)
SC_fgetfileinfo         equ 24  ; fgetfileinfo(int fd, struct _file_info_t* info)
SC_fsetfileinfo         equ 25  ; fsetfileinfo(int fd, struct _mutable_file_info_t* info)
SC_unlink               equ 26  ; errno_t unlink(const char* path)
SC_rename               equ 27  ; errno_t rename(const char* oldpath, const char* newpath)
SC_ioctl                equ 28  ; errno_t ioctl(int fd, int cmd, ...)
SC_truncate             equ 29  ; errno_t truncate(const char* path, off_t length)
SC_ftruncate            equ 30  ; errno_t ftruncate(int fd, off_t length)
SC_mkfile               equ 31  ; errno_t creat(const char* _Nonnull path, int options, int permissions, int* _Nonnull fd)

SC_numberOfCalls        equ 32   ; number of system calls


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