//
//  Semaphore.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/10/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Semaphore.h"
#include <driver/InterruptController.h>
#include "VirtualProcessorScheduler.h"


// Creates a new semaphore with the given starting value.
Semaphore* _Nullable Semaphore_Create(int value)
{
    Semaphore* pSemaphore;
    
    if (kalloc(sizeof(Semaphore), (void**) &pSemaphore) == EOK) {
        Semaphore_Init(pSemaphore, value);
    }
    return pSemaphore;
}

void Semaphore_Destroy(Semaphore* _Nullable pSemaphore)
{
    if (pSemaphore) {
        Semaphore_Deinit(pSemaphore);
        kfree(pSemaphore);
    }
}

// Initializes a new semaphore with 'value' permits
void Semaphore_Init(Semaphore* _Nonnull pSemaphore, int value)
{
    pSemaphore->value = value;
    List_Init(&pSemaphore->wait_queue);
}

// Deinitializes the semaphore. All virtual processors that are still waiting
// for permits on this semaphore are woken up with an EINTR error.
void Semaphore_Deinit(Semaphore* _Nonnull pSemaphore)
{
    if (!List_IsEmpty(&pSemaphore->wait_queue)) {
        // Wake up all the guys that are still waiting on us and tell them that the
        // wait has been interrupted.
        const int sps = VirtualProcessorScheduler_DisablePreemption();
        VirtualProcessorScheduler_WakeUpSome(gVirtualProcessorScheduler,
                                             &pSemaphore->wait_queue,
                                             INT_MAX,
                                             WAKEUP_REASON_INTERRUPTED,
                                             true);
        VirtualProcessorScheduler_RestorePreemption(sps);
    }

    List_Deinit(&pSemaphore->wait_queue);
}

void Semaphore_Release(Semaphore* _Nonnull pSemaphore)
{
    Semaphore_ReleaseMultiple(pSemaphore, 1);
}

errno_t Semaphore_Acquire(Semaphore* _Nonnull pSemaphore, TimeInterval deadline)
{
    return Semaphore_AcquireMultiple(pSemaphore, 1, deadline);
}

bool Semaphore_TryAcquire(Semaphore* _Nonnull pSemaphore)
{
    return Semaphore_TryAcquireMultiple(pSemaphore, 1);
}

// Invoked by Semaphore_Acquire() if the semaphore doesn't have the expected number
// of permits.
// Expects to be called with preemption disabled.
errno_t Semaphore_OnWaitForPermits(Semaphore* _Nonnull pSemaphore, TimeInterval deadline)
{
    return VirtualProcessorScheduler_WaitOn(gVirtualProcessorScheduler,
                                            &pSemaphore->wait_queue,
                                            deadline,
                                            true);
}

// Invoked by Semaphore_Release().
// Expects to be called with preemption disabled.
void Semaphore_WakeUp(Semaphore* _Nullable pSemaphore)
{
    VirtualProcessorScheduler_WakeUpAll(gVirtualProcessorScheduler,
                                        &pSemaphore->wait_queue,
                                        true);
}