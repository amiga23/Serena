//
//  syscall.h
//  libsystem
//
//  Created by Dietmar Planitzer on 9/2/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_SYSCALL_H
#define _SYS_SYSCALL_H 1

#include <System/_cmndef.h>
#include <System/abi/_inttypes.h>

__CPP_BEGIN

enum {
    SC_read = 0,            // errno_t IOChannel_Read(int fd, const char * _Nonnull buffer, size_t nBytesToRead, ssize_t* pOutBytesRead)
    SC_write,               // errno_t IOChannel_Write(int fd, const char * _Nonnull buffer, size_t nBytesToWrite, ssize_t* pOutBytesWritten)
    SC_delay,               // errno_t Delay(TimeInterval ti)
    SC_dispatch,            // errno_t _DispatchQueue_Dispatch(int od, unsigned long options, Dispatch_Closure _Nonnull pUserClosure, void* _Nullable pContext)
    SC_alloc_address_space, // errno_t Process_AllocateAddressSpace(int nbytes, void **pOutMem)
    SC_exit,                // _Noreturn Process_Exit(int status)
    SC_spawn_process,       // errno_t Process_Spawn(SpawnArguments * _Nonnull args, ProcessId * _Nullable rpid)
    SC_getpid,              // ProcessId Process_GetId(void)
    SC_getppid,             // ProcessId Process_GetParentId(void)
    SC_getpargs,            // ProcessArguments * _Nonnull Process_GetArguments(void)
    SC_open,                // errno_t File_Open(const char * _Nonnull name, int options, int* _Nonnull fd)
    SC_close,               // errno_t IOChannel_Close(int fd)
    SC_waitpid,             // errno_t Process_WaitForChildTermination(ProcessId pid, ProcessTerminationStatus * _Nullable result)
    SC_seek,                // errno_t File_Seek(int fd, FileOffset offset, FileOffset * _Nullable newpos, int whence)
    SC_getcwd,              // errno_t Process_GetWorkingDirectory(char* buffer, size_t bufferSize)
    SC_setcwd,              // errno_t Process_SetWorkingDirectory(const char* _Nonnull path)
    SC_getuid,              // UserId Process_GetUserId(void)
    SC_getumask,            // FilePermissions Process_GetUserMask(void)
    SC_setumask,            // void Process_SetUserMask(FilePermissions mask)
    SC_mkdir,               // errno_t Directory_Create(const char* _Nonnull path, FilePermissions mode)
    SC_getfileinfo,         // errno_t File_GetInfo(const char* _Nonnull path, FileInfo* _Nonnull info)
    SC_opendir,             // errno_t Directory_Open(const char* _Nonnull path, int* _Nonnull fd)
    SC_setfileinfo,         // errno_t File_SetInfo(const char* _Nonnull path, MutableFileInfo* _Nonnull info)
    SC_access,              // errno_t File_CheckAccess(const char* _Nonnull path, int mode)
    SC_fgetfileinfo,        // errno_t FileChannel_GetInfo(int fd, FileInfo* _Nonnull info)
    SC_fsetfileinfo,        // errno_t FileChannel_SetInfo(int fd, MutableFileInfo* _Nonnull info)
    SC_unlink,              // errno_t File_Unlink(const char* path)
    SC_rename,              // errno_t rename(const char* _Nonnull oldpath, const char* _Nonnull newpath)
    SC_ioctl,               // errno_t IOChannel_Control(int fd, int cmd, ...)
    SC_truncate,            // errno_t File_Truncate(const char* _Nonnull path, FileOffset length)
    SC_ftruncate,           // errno_t FileChannel_Truncate(int fd, FileOffset length)
    SC_mkfile,              // errno_t File_Create(const char* _Nonnull path, int options, int permissions, int* _Nonnull fd)
    SC_mkpipe,              // errno_t Pipe_Create(int* _Nonnull rioc, int* _Nonnull wioc)
    SC_dispatch_after,      // errno_t DispatchQueue_DispatchAsyncAfter(int od, TimeInterval deadline, Dispatch_Closure _Nonnull pClosure, void* _Nullable pContext)
    SC_dispatch_queue_create,   // errno_t DispatchQueue_Create(int minConcurrency, int maxConcurrency, int qos, int priority, int* _Nonnull pOutQueue)
    SC_dispatch_queue_current,  // int DispatchQueue_GetCurrent(void)
    SC_dispose,             // _Object_Dispose(int od)
    SC_get_monotonic_time,  // TimeInterval MonotonicClock_GetTime(void)
};


extern intptr_t _syscall(int scno, ...);

__CPP_END

#endif /* _SYS_SYSCALL_H */
