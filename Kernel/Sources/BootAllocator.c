//
//  BootAllocator.c
//  Apollo
//
//  Created by Dietmar Planitzer on 8/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "BootAllocator.h"


void BootAllocator_Init(BootAllocator* _Nonnull pAlloc, SystemDescription* _Nonnull pSysDesc)
{
    assert(pSysDesc->memory.descriptor_count > 0);
    pAlloc->mem_descs = pSysDesc->memory.descriptor;
    pAlloc->current_desc_index = pSysDesc->memory.descriptor_count - 1;
    pAlloc->current_top = align_down_byte_ptr(pAlloc->mem_descs[pAlloc->current_desc_index].upper, CPU_PAGE_SIZE);
}

void BootAllocator_Deinit(BootAllocator* _Nonnull pAlloc)
{
    pAlloc->mem_descs = NULL;
    pAlloc->current_top = NULL;
}

// Allocates a memory block from CPU-only RAM that is able to hold at least 'nbytes'.
// Note that the base address of the allocated block is page aligned. Never returns
// NULL.
Byte* _Nonnull BootAllocator_Allocate(BootAllocator* _Nonnull pAlloc, Int nbytes)
{
    assert(nbytes > 0);
    Byte* ptr = NULL;

    while(true) {
        ptr = align_down_byte_ptr(pAlloc->current_top - nbytes, CPU_PAGE_SIZE);
        if (ptr >= pAlloc->mem_descs[pAlloc->current_desc_index].lower) {
            pAlloc->current_top = ptr;
            return ptr;
        }

        assert(pAlloc->current_desc_index > 0);
        pAlloc->current_desc_index--;
        pAlloc->current_top = align_down_byte_ptr(pAlloc->mem_descs[pAlloc->current_desc_index].upper, CPU_PAGE_SIZE);
    }
}

// Returns the lowest address used by the boot allocator. This address is always
// page aligned.
Byte* _Nonnull BootAllocator_GetLowestAllocatedAddress(BootAllocator* _Nonnull pAlloc)
{
    // current_top is always page aligned in the existing implementation
    return pAlloc->current_top;
}