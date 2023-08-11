//
//  DispatchQueuePriv.h
//  Apollo
//
//  Created by Dietmar Planitzer on 8/10/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef DispatchQueuePriv_h
#define DispatchQueuePriv_h

#include "DispatchQueue.h"
#include "kalloc.h"
#include "ConditionVariable.h"
#include "List.h"
#include "Lock.h"
#include "MonotonicClock.h"
#include "Semaphore.h"
#include "VirtualProcessorScheduler.h"


enum ItemType {
    kItemType_Immediate = 0,    // Execute the item as soon as possible
    kItemType_OneShotTimer,     // Execute the item once on or after its deadline
    kItemType_RepeatingTimer,   // Execute the item on or after its deadline and then reschedule it for the next deadline
};


//
// Work Items
//

typedef struct _WorkItem {
    SListNode                   queue_entry;
    DispatchQueueClosure        closure;
    Semaphore * _Nullable _Weak completion_sema;
    Bool                        is_owned_by_queue;      // item was created and is owned by the dispatch queue and thus is eligble to be moved to the work item cache
    AtomicBool                  is_being_dispatched;    // shared between all dispatch queues (set to true while the work item is in the process of being dispatched by a queue; false if no queue is using it)
    AtomicBool                  cancelled;              // shared between dispatch queue and queue user
    Int8                        type;
} WorkItem;




//
// Timers
//

typedef struct _Timer {
    WorkItem        item;
    TimeInterval    deadline;           // Time when the timer closure should be executed
    TimeInterval    interval;
} Timer;


//
// Completion Signaler
//

// Completion signalers are semaphores that are used to signal the completion of
// a work item to DispatchQueue_DispatchSync()
typedef struct _CompletionSignaler {
    SListNode   queue_entry;
    Semaphore   semaphore;
} CompletionSignaler;


//
// Dispatch Queue
//

// A concurrency lane is a virtual processor and all associated resources. The
// resources are specific to this virtual processor and shall only be used in
// connection with this virtual processor. There's one concurrency lane per
// dispatch queue concurrency level.
typedef struct _ConcurrencyLane {
    VirtualProcessor* _Nullable  vp;     // The virtual processor assigned to this concurrency lane
} ConcurrencyLane;


enum QueueState {
    kQueueState_Running,                // Queue is running and willing to accept and execute closures
    kQueueState_Terminating,            // DispatchQueue_Terminate() was called and the queue is in the process of terminating
    kQueueState_Terminated              // The queue has finished terminating. All virtual processors are relinquished
};


#define MAX_ITEM_CACHE_COUNT    8
#define MAX_TIMER_CACHE_COUNT   8
#define MAX_COMPLETION_SIGNALER_CACHE_COUNT 8
typedef struct _DispatchQueue {
    SList                               item_queue;         // Queue of work items that should be executed as soon as possible
    SList                               timer_queue;        // Queue of items that should be executed on or after their deadline
    SList                               item_cache_queue;   // Cache of reusable work items
    SList                               timer_cache_queue;  // Cache of reusable timers
    SList                               completion_signaler_cache_queue;    // Cache of reusable completion signalers
    Lock                                lock;
    ConditionVariable                   work_available_signaler;    // Used by the queue to indicate to its VPs that a new work item/timer has bbeen enqueued
    ConditionVariable                   vp_shutdown_signaler;       // Used by a VP to indicate that it has relinqushed itself because the queue is in the process of shutting down
    ProcessRef _Nullable _Weak          owning_process;             // The process that owns this queue
    VirtualProcessorPoolRef _Nonnull    virtual_processor_pool;     // Pool from which the queue should retrieve virtual processors
    Int                                 items_queued_count;         // Number of work items queued up (item_queue)
    Int8                                state;                      // The current dispatch queue state
    Int8                                maxConcurrency;             // Maximum number of concurrency lanes we are allowed to allocate and use
    Int8                                availableConcurrency;       // Number of concurrency lanes we have acquired and are available for use
    Int8                                qos;
    Int8                                priority;
    Int8                                item_cache_count;
    Int8                                timer_cache_count;
    Int8                                completion_signaler_count;
    ConcurrencyLane                     concurrency_lanes[1];   // Up to 'maxConcurrency' concurrency lanes
} DispatchQueue;

 
#endif /* DispatchQueuePriv_h */
