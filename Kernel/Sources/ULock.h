//
//  ULock.h
//  Apollo
//
//  Created by Dietmar Planitzer on 8/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef ULock_h
#define ULock_h

#include "Foundation.h"
#include "List.h"


typedef struct _ULock {
    volatile UInt   value;
    List            wait_queue;
    Int             owner_vpid;     // ID of the VP that is currently holding the lock
} ULock;


// ULock_Lock options
// Marks a ULock_Lock as interruptable. This should be used for locks that are
// used by user space code. Kernel space locks should not be interruptable.
#define ULOCK_OPTION_INTERRUPTABLE  1 


// Initializes a new lock.
extern void ULock_Init(ULock*_Nonnull pLock);

// Deinitializes a lock. The lock is automatically unlocked if the calling code
// is holding the lock.
extern ErrorCode ULock_Deinit(ULock* _Nonnull pLock);

// Allocates a new lock.
extern ErrorCode _Nullable ULock_Create(ULock* _Nullable * _Nonnull pOutLock);

// Deallocates a lock. The lock is automatically unlocked if the calling code
// is holding the lock.
extern ErrorCode ULock_Destroy(ULock* _Nullable pLock);

// Tries locking the given lock. EOK is returned if the lock attempt succeeded
// and EBUSY otherwise.
extern ErrorCode ULock_TryLock(ULock* _Nonnull pLock);

// Blocks the caller until the lock can be taken successfully. Note that the
// wait may be interrupted with an EINTR if the interruptable option is used. A
// non-interruptable wait will not return with an EINTR. Interruptable waits
// should be used for userspace related locks and non-interruptable waits for
// locks that are created and owned by the kernel.
extern ErrorCode ULock_Lock(ULock* _Nonnull pLock, UInt options);

// Unlocks the lock.
extern ErrorCode ULock_Unlock(ULock* _Nonnull pLock);

// Returns the ID of the virtual processor that is currently holding the lock.
// Zero is returned if none is holding the lock.
extern Int ULock_GetOwnerVpid(ULock* _Nonnull pLock);

#endif /* ULock_h */
