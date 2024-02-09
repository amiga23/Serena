//
//  MonotonicClock.c
//  Apollo
//
//  Created by Dietmar Planitzer on 2/11/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "MonotonicClock.h"
#include "Platform.h"
#include "InterruptController.h"

#define ONE_SECOND_IN_NANOS (1000 * 1000 * 1000)

static void MonotonicClock_OnInterrupt(MonotonicClock* _Nonnull pClock);

MonotonicClock  gMonotonicClockStorage;
MonotonicClock* gMonotonicClock = &gMonotonicClockStorage;


// CIA timer usage:
// CIA B timer A: monotonic clock tick counter


// Initializes the monotonic clock. The monotonic clock uses the quantum timer
// as its time base.
errno_t MonotonicClock_CreateForLocalCPU(const SystemDescription* pSysDesc)
{
    MonotonicClock* pClock = &gMonotonicClockStorage;
    decl_try_err();

    pClock->current_time = kTimeInterval_Zero;
    pClock->current_quantum = 0;
    pClock->ns_per_quantum = pSysDesc->quantum_duration_ns;

    InterruptHandlerID irqHandler;
    try(InterruptController_AddDirectInterruptHandler(gInterruptController,
                                                      INTERRUPT_ID_QUANTUM_TIMER,
                                                      INTERRUPT_HANDLER_PRIORITY_HIGHEST,
                                                      (InterruptHandler_Closure)MonotonicClock_OnInterrupt,
                                                      pClock,
                                                      &irqHandler));
    InterruptController_SetInterruptHandlerEnabled(gInterruptController, irqHandler, true);

    chipset_start_quantum_timer();
    return EOK;

catch:
    return err;
}

// Returns the current time of the clock in terms of microseconds.
TimeInterval MonotonicClock_GetCurrentTime(void)
{
    register const MonotonicClock* pClock = gMonotonicClock;
    register int32_t cur_secs, cur_nanos;
    register int32_t chk_quantum;
    
    do {
        cur_secs = pClock->current_time.seconds;
        cur_nanos = pClock->current_time.nanoseconds;
        chk_quantum = pClock->current_quantum;
        
        cur_nanos += chipset_get_quantum_timer_elapsed_ns();
        if (cur_nanos >= ONE_SECOND_IN_NANOS) {
            cur_secs++;
            cur_nanos -= ONE_SECOND_IN_NANOS;
        }
        
        // Do it again if there was a quantum transition while we were busy computing
        // the time
    } while (pClock->current_quantum != chk_quantum);

    return TimeInterval_Make(cur_secs, cur_nanos);
}

static void MonotonicClock_OnInterrupt(MonotonicClock* _Nonnull pClock)
{
    // update the scheduler clock
    pClock->current_quantum++;
    
    
    // update the metric time
    pClock->current_time.nanoseconds += pClock->ns_per_quantum;
    if (pClock->current_time.nanoseconds >= ONE_SECOND_IN_NANOS) {
        pClock->current_time.seconds++;
        pClock->current_time.nanoseconds -= ONE_SECOND_IN_NANOS;
    }
}

// Blocks the caller until 'deadline'. Returns true if the function did the
// necessary delay and false if the caller should do something else instead to
// achieve the desired delay. Eg context switch to another virtual processor.
// Note that this function is only willing to block the caller for at most a
// millisecond. Longer delays should be done via a scheduler wait().
bool MonotonicClock_DelayUntil(TimeInterval deadline)
{
    const TimeInterval t_start = MonotonicClock_GetCurrentTime();
    const TimeInterval t_delta = TimeInterval_Subtract(deadline, t_start);
    
    if (t_delta.seconds > 0 || (t_delta.seconds == 0 && t_delta.nanoseconds > 1000*1000)) {
        return false;
    }
    
    // Just spin for now (would be nice to put the CPU to sleep though for a few micros before rechecking the time or so)
    for (;;) {
        const TimeInterval t_cur = MonotonicClock_GetCurrentTime();
        
        if (TimeInterval_GreaterEquals(t_cur, deadline)) {
            return true;
        }
    }
    
    // not reached
    return true;
}

// Converts a time interval to a quantum value. The quantum value is rounded
// based on the 'rounding' parameter.
Quantums Quantums_MakeFromTimeInterval(TimeInterval ti, int rounding)
{
    register MonotonicClock* pClock = gMonotonicClock;
    const int64_t nanos = (int64_t)ti.seconds * (int64_t)ONE_SECOND_IN_NANOS + (int64_t)ti.nanoseconds;
    const int64_t quants = nanos / (int64_t)pClock->ns_per_quantum;
    
    switch (rounding) {
        case QUANTUM_ROUNDING_TOWARDS_ZERO:
            return quants;
            
        case QUANTUM_ROUNDING_AWAY_FROM_ZERO: {
            const int64_t nanos_prime = quants * (int64_t)pClock->ns_per_quantum;
            
            return (nanos_prime < nanos) ? (int32_t)quants + 1 : (int32_t)quants;
        }
            
        default:
            abort();
            return 0;
    }
}

// Converts a quantum value to a time interval.
TimeInterval TimeInterval_MakeFromQuantums(Quantums quants)
{
    register MonotonicClock* pClock = gMonotonicClock;
    const int64_t ns = (int64_t)quants * (int64_t)pClock->ns_per_quantum;
    const int32_t secs = ns / (int64_t)ONE_SECOND_IN_NANOS;
    const int32_t nanos = ns - ((int64_t)secs * (int64_t)ONE_SECOND_IN_NANOS);
    
    return TimeInterval_Make(secs, nanos);
}
