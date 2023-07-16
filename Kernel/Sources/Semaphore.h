//
//  Semaphore.h
//  Apollo
//
//  Created by Dietmar Planitzer on 2/10/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Semaphore_h
#define Semaphore_h

#include "Foundation.h"
#include "List.h"
#include "MonotonicClock.h"


// A (counting) semaphore
typedef struct _Semaphore {
    volatile Int    value;
    List            wait_queue;
} Semaphore;


extern Semaphore* _Nullable Semaphore_Create(Int value);
extern void Semaphore_Destroy(Semaphore* _Nullable pSemaphore);

// Initializes a new semaphore with 'value' permits
extern void Semaphore_Init(Semaphore* _Nonnull pSemaphore, Int value);

// Deinitializes the semaphore. All virtual processors that are still waiting
// for permits on this semaphore are woken up with an EINTR error.
extern void Semaphore_Deinit(Semaphore* _Nonnull pSemaphore);

extern void Semaphore_Release(Semaphore* _Nonnull pSemaphore);
extern void Semaphore_ReleaseMultiple(Semaphore* _Nonnull sema, Int npermits);

// Blocks the caller until the semaphore has at least one permit available or
// the wait has timed out. Note that this function may return EINTR which means
// that the Semaphore_Acquire() call is happening in the context of a system
// call that should be aborted.
extern ErrorCode Semaphore_Acquire(Semaphore* _Nonnull pSemaphore, TimeInterval deadline);
extern ErrorCode Semaphore_AcquireMultiple(Semaphore* _Nonnull sema, Int npermits, TimeInterval deadline);
extern ErrorCode Semaphore_AcquireAll(Semaphore* _Nonnull pSemaphore, TimeInterval deadline, Int* _Nonnull pOutPermitCount);

extern Bool Semaphore_TryAcquire(Semaphore* _Nonnull pSemaphore);
extern Bool Semaphore_TryAcquireMultiple(Semaphore* _Nonnull pSemaphore, Int npermits);
extern Int Semaphore_TryAcquireAll(Semaphore* _Nonnull pSemaphore);

#endif /* Semaphore_h */
