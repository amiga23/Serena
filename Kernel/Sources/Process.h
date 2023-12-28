//
//  Process.h
//  Apollo
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef Process_h
#define Process_h

#include <klib/klib.h>
#include "Filesystem.h"

OPAQUE_CLASS(Process, Object);
typedef struct _ProcessMethodTable {
    ObjectMethodTable   super;
} ProcessMethodTable;

// The process spawn arguments specify how a child process should be created.
typedef struct __spawn_arguments_t SpawnArguments;

// The process termination status generated when a child process terminates.
typedef struct __waitpid_result_t ProcessTerminationStatus;

extern ProcessRef _Nonnull  gRootProcess;


// Returns the process associated with the calling execution context. Returns
// NULL if the execution context is not associated with a process. This will
// never be the case inside of a system call.
extern ProcessRef _Nullable Process_GetCurrent(void);


// Creates the root process which is the first process of the OS.
extern ErrorCode RootProcess_Create(ProcessRef _Nullable * _Nonnull pOutProc);

// Loads an executable from the given executable file into the process address
// space. This is only meant to get the root process going.
// \param pProc the process into which the executable image should be loaded
// \param pExecAddr pointer to a GemDOS formatted executable file in memory
// XXX expects that the address space is empty at call time
// XXX the executable format is GemDOS
// XXX the executable file must be located at the address 'pExecAddr'
extern ErrorCode RootProcess_Exec(ProcessRef _Nonnull pProc, Byte* _Nonnull pExecAddr);


// Triggers the termination of the given process. The termination may be caused
// voluntarily (some VP currently owned by the process triggers this call) or
// involuntarily (some other process triggers this call). Note that the actual
// termination is done asynchronously. 'exitCode' is the exit code that should
// be made available to the parent process. Note that the only exit code that
// is passed to the parent is the one from the first Process_Terminate() call.
// All others are discarded.
extern void Process_Terminate(ProcessRef _Nonnull pProc, Int exitCode);

// Returns true if the process is marked for termination and false otherwise.
extern Bool Process_IsTerminating(ProcessRef _Nonnull pProc);

// Waits for the child process with the given PID to terminate and returns the
// termination status. Returns ECHILD if there are no tombstones of terminated
// child processes available or the PID is not the PID of a child process of
// the receiver. Otherwise blocks the caller until the requested process or any
// child process (pid == -1) has exited.
extern ErrorCode Process_WaitForTerminationOfChild(ProcessRef _Nonnull pProc, ProcessId pid, ProcessTerminationStatus* _Nullable pStatus);

extern Int Process_GetId(ProcessRef _Nonnull pProc);
extern Int Process_GetParentId(ProcessRef _Nonnull pProc);

extern UserId Process_GetRealUserId(ProcessRef _Nonnull pProc);

// Returns the base address of the process arguments area. The address is
// relative to the process address space.
extern void* Process_GetArgumentsBaseAddress(ProcessRef _Nonnull pProc);

// Spawns a new process that will be a child of the given process. The spawn
// arguments specify how the child process should be created, which arguments
// and environment it will receive and which descriptors it will inherit.
extern ErrorCode Process_SpawnChildProcess(ProcessRef _Nonnull pProc, const SpawnArguments* _Nonnull pArgs, ProcessId * _Nullable pOutChildPid);

extern ErrorCode Process_DispatchAsyncUser(ProcessRef _Nonnull pProc, Closure1Arg_Func pUserClosure);

// Allocates more (user) address space to the given process.
extern ErrorCode Process_AllocateAddressSpace(ProcessRef _Nonnull pProc, ByteCount count, Byte* _Nullable * _Nonnull pOutMem);


// Registers the given I/O channel with the process. This action allows the
// process to use this I/O channel. The process maintains a strong reference to
// the channel until it is unregistered. Note that the process retains the
// channel and thus you have to release it once the call returns. The call
// returns a descriptor which can be used to refer to the channel from user
// and/or kernel space.
extern ErrorCode Process_RegisterIOChannel(ProcessRef _Nonnull pProc, IOChannelRef _Nonnull pChannel, Int* _Nonnull pOutDescriptor);

// Unregisters the I/O channel identified by the given descriptor. The channel
// is removed from the process' I/O channel table and a strong reference to the
// channel is returned. The caller should call close() on the channel to close
// it and then release() to release the strong reference to the channel. Closing
// the channel will mark itself as done and the channel will be deallocated once
// the last strong reference to it has been released.
extern ErrorCode Process_UnregisterIOChannel(ProcessRef _Nonnull pProc, Int fd, IOChannelRef _Nullable * _Nonnull pOutChannel);

// Looks up the I/O channel identified by the given descriptor and returns a
// strong reference to it if found. The caller should call release() on the
// channel once it is no longer needed.
extern ErrorCode Process_CopyIOChannelForDescriptor(ProcessRef _Nonnull pProc, Int fd, IOChannelRef _Nullable * _Nonnull pOutChannel);


// Sets the receiver's root directory to the given path. Note that the path must
// point to a directory that is a child or the current root directory of the
// process.
extern ErrorCode Process_SetRootDirectoryPath(ProcessRef _Nonnull pProc, const Character* pPath);

// Sets the receiver's current working directory to the given path.
extern ErrorCode Process_SetCurrentWorkingDirectoryPath(ProcessRef _Nonnull pProc, const Character* _Nonnull pPath);

// Returns the current working directory in the form of a path. The path is
// written to the provided buffer 'pBuffer'. The buffer size must be at least as
// large as length(path) + 1.
extern ErrorCode Process_GetCurrentWorkingDirectoryPath(ProcessRef _Nonnull pProc, Character* _Nonnull pBuffer, ByteCount bufferSize);

// Returns the file creation mask of the receiver. Bits cleared in this mask
// should be removed from the file permissions that user space sent to create a
// file system object (note that this is the compliment of umask).
extern FilePermissions Process_GetFileCreationMask(ProcessRef _Nonnull pProc);

// Sets the file creation mask of the receiver.
extern void Process_SetFileCreationMask(ProcessRef _Nonnull pProc, FilePermissions mask);

// Opens the given file or named resource. Opening directories is handled by the
// Process_OpenDirectory() function.
extern ErrorCode Process_Open(ProcessRef _Nonnull pProc, const Character* _Nonnull pPath, UInt options, Int* _Nonnull pOutDescriptor);

// Creates a new directory. 'permissions' are the file permissions that should be
// assigned to the new directory (modulo the file creation mask).
extern ErrorCode Process_CreateDirectory(ProcessRef _Nonnull pProc, const Character* _Nonnull pPath, FilePermissions permissions);

// Opens the directory at the given path and returns an I/O channel that represents
// the open directory.
extern ErrorCode Process_OpenDirectory(ProcessRef _Nonnull pProc, const Character* _Nonnull pPath, Int* _Nonnull pOutDescriptor);

// Returns information about the file at the given path.
extern ErrorCode Process_GetFileInfo(ProcessRef _Nonnull pProc, const Character* _Nonnull pPath, FileInfo* _Nonnull pOutInfo);

// Same as above but with respect to the given I/O channel.
extern ErrorCode Process_GetFileInfoFromIOChannel(ProcessRef _Nonnull pProc, Int fd, FileInfo* _Nonnull pOutInfo);

// Modifies information about the file at the given path.
extern ErrorCode Process_SetFileInfo(ProcessRef _Nonnull pProc, const Character* _Nonnull pPath, MutableFileInfo* _Nonnull pInfo);

// Same as above but with respect to the given I/O channel.
extern ErrorCode Process_SetFileInfoFromIOChannel(ProcessRef _Nonnull pProc, Int fd, MutableFileInfo* _Nonnull pInfo);

// Sets the length of an existing file. The file may either be reduced in size
// or expanded.
extern ErrorCode Process_TruncateFile(ProcessRef _Nonnull pProc, const Character* _Nonnull pPath, FileOffset length);

// Same as above but the file is identified by the given I/O channel.
extern ErrorCode Process_TruncateFileFromIOChannel(ProcessRef _Nonnull pProc, Int fd, FileOffset length);

// Sends a I/O Channel or I/O Resource defined command to the I/O Channel or
// resource identified by the given descriptor.
extern ErrorCode Process_vIOControl(ProcessRef _Nonnull pProc, Int fd, Int cmd, va_list ap);

// Returns EOK if the given file is accessible assuming the given access mode;
// returns a suitable error otherwise. If the mode is 0, then a check whether the
// file exists at all is executed.
extern ErrorCode Process_CheckFileAccess(ProcessRef _Nonnull pProc, const Character* _Nonnull pPath, Int mode);

// Unlinks the inode at the path 'pPath'.
extern ErrorCode Process_Unlink(ProcessRef _Nonnull pProc, const Character* _Nonnull pPath);

// Renames the file or directory at 'pOldPath' to the new location 'pNewPath'.
extern ErrorCode Process_Rename(ProcessRef _Nonnull pProc, const Character* pOldPath, const Character* pNewPath);

#endif /* Process_h */
