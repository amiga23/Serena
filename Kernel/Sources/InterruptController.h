//
//  InterruptController.h
//  Apollo
//
//  Created by Dietmar Planitzer on 5/5/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef InterruptController_h
#define InterruptController_h

#include "Foundation.h"
#include "Platform.h"
#include "Lock.h"
#include "Semaphore.h"


#define INTERRUPT_HANDLER_PRIORITY_LOWEST   -128
#define INTERRUPT_HANDLER_PRIORITY_NORMAL   0
#define INTERRUPT_HANDLER_PRIORITY_HIGHEST   127


// An interrupt ID
typedef Int InterruptID;

// The ID that represents a specifc registered interrupt handler
typedef Int InterruptHandlerID;

// Closure which is invoked when an interrupt happens
typedef void (*InterruptHandler_Closure)(Byte* _Nullable pContext);


struct _InterruptHandlerArray;
struct _InterruptController;
typedef struct _InterruptController* InterruptControllerRef;


extern ErrorCode InterruptController_Init(InterruptControllerRef _Nonnull pController);


// Registers a direct interrupt handler. The interrupt controller will invoke the
// given closure with the given context every time an interrupt with ID 'interruptId'
// is triggered.
// NOTE: The closure is invoked in the interrupt context.
extern ErrorCode InterruptController_AddDirectInterruptHandler(InterruptControllerRef _Nonnull pController, InterruptID interruptId, Int priority, InterruptHandler_Closure _Nonnull pClosure, Byte* _Nullable pContext, InterruptHandlerID* _Nonnull pOutId);

// Registers a counting semaphore which will receive a release call for every
// occurence of an interrupt with ID 'interruptId'.
extern ErrorCode InterruptController_AddSemaphoreInterruptHandler(InterruptControllerRef _Nonnull pController, InterruptID interruptId, Int priority, Semaphore* _Nonnull pSemaphore, InterruptHandlerID* _Nonnull pOutId);

// Removes the interrupt handler for the given handler ID. Does nothing if no
// such handler is registered.
extern ErrorCode InterruptController_RemoveInterruptHandler(InterruptControllerRef _Nonnull pController, InterruptHandlerID handlerId);

// Enables / disables the interrupt handler with the given interrupt handler ID.
// Note that interrupt handlers are by default disabled (when you add them). You
// need to enable an interrupt handler before it is able to respond to interrupt
// requests. A disabled interrupt handler ignores interrupt requests.
extern ErrorCode InterruptController_SetInterruptHandlerEnabled(InterruptControllerRef _Nonnull pController, InterruptHandlerID handlerId, Bool enabled);

// Returns true if the given interrupt handler is enabled; false otherwise.
extern Bool InterruptController_IsInterruptHandlerEnabled(InterruptControllerRef _Nonnull pController, InterruptHandlerID handlerId);

// Called by the low-level interrupt handler code. Invokes the interrupt handlers
// for the given interrupt
extern void InterruptController_OnInterrupt(struct _InterruptHandlerArray* _Nonnull pArray);

// Returns the number of spurious interrupts that have happened since boot. A
// spurious interrupt is an interrupt request that was not acknowledged by the
// hardware.
extern Int InterruptController_GetSpuriousInterruptCount(InterruptControllerRef _Nonnull pController);

// Returns true if the caller is running in the interrupt context and false otherwise.
extern Bool InterruptController_IsServicingInterrupt(InterruptControllerRef _Nonnull pController);

extern void InterruptController_Dump(InterruptControllerRef _Nonnull pController);


// Internal
#define INTERRUPT_HANDLER_TYPE_DIRECT               0
#define INTERRUPT_HANDLER_TYPE_COUNTING_SEMAPHORE   1

#define INTERRUPT_HANDLER_FLAG_ENABLED  0x01

typedef struct _InterruptHandler {
    Int     identity;
    Int8    type;
    Int8    priority;
    UInt8   flags;
    Int8    reserved;
    union {
        struct {
            InterruptHandler_Closure _Nonnull   closure;
            Byte* _Nullable                     context;
        }   direct;
        struct {
            Semaphore* _Nonnull semaphore;
        }   sema;
    }       u;
} InterruptHandler;


typedef struct _InterruptHandlerArray {
    InterruptHandler* _Nonnull  data;
    Int                         size;
} InterruptHandlerArray;


typedef struct _InterruptController {
    InterruptHandlerArray   handlers[INTERRUPT_ID_COUNT];
    Int                     nextAvailableId;    // Next available interrupt handler ID
    Int                     spuriousInterruptCount;
    Int8                    isServicingInterrupt;   // > 0 while we are running in the IRQ context; == 0 if we are running outside the IRQ context
    Int8                    reserved[3];
    Lock                    lock;
} InterruptController;

#endif /* InterruptController_h */
