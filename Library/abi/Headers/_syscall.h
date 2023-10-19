//
//  _syscalldef.h
//  Apollo
//
//  Created by Dietmar Planitzer on 9/2/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef __SYSCALLS_H
#define __SYSCALLS_H 1

#define SC_read                 0   // ssize_t read(int fd, const char * _Nonnull buffer, size_t count)
#define SC_write                1   // ssize_t write(int fd, const char * _Nonnull buffer, size_t count)
#define SC_sleep                2   // errno_t sleep(struct {int secs, int nanosecs})
#define SC_dispatch_async       3   // errno_t dispatch_async(void * _Nonnull pUserClosure)
#define SC_alloc_address_space  4   // errno_t alloc_address_space(int nbytes, void **pOutMem)
#define SC_exit                 5   // _Noreturn exit(int status)
#define SC_spawn_process        6   // errno_t spawn_process(void *pExecBase, const char *const *argv, const char *const *envp) [XXX argv and envp may be NULL in user space, user space should pass {path, NULL} if argv == NULL and 'environ' if envp == NULL]
#define SC_getpid               7   // pid_t getpid(void)
#define SC_getppid              8   // pid_t getppid(void)
#define SC_getpargs             9   // struct __process_arguments_t * _Nonnull getpargs(void)
#define SC_open                 10  // int open(const char * _Nonnull name, int options)
#define SC_close                11  // errno_t close(int fd)

#endif /* __SYSCALLS_H */
