//
//  Bits.c
//  Apollo
//
//  Created by Dietmar Planitzer on 3/9/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Bits.h"
#include "Bytes.h"
#include "Log.h"


static inline Int index_of_first_1_in_byte(Byte byte, Int low_idx_incl, Int high_idx_incl)
{
    for (Int i = low_idx_incl; i <= high_idx_incl; i++) {
        if ((byte & (1 << (7 - i))) != 0) {
            return i;
        }
    }
    
    return -1;
}

static inline Int index_of_last_1_in_byte(Byte byte, Int high_idx_incl, Int low_idx_incl)
{
    for (Int i = high_idx_incl; i >= low_idx_incl; i--) {
        if ((byte & (1 << (7 - i))) != 0) {
            return i;
        }
    }
    
    return -1;
}

static inline Int index_of_first_0_in_byte(Byte byte, Int low_idx_incl, Int high_idx_incl)
{
    for (Int i = low_idx_incl; i <= high_idx_incl; i++) {
        if ((byte & (1 << (7 - i))) == 0) {
            return i;
        }
    }
    
    return -1;
}

static inline Int index_of_last_0_in_byte(Byte byte, Int high_idx_incl, Int low_idx_incl)
{
    for (Int i = high_idx_incl; i >= low_idx_incl; i--) {
        if ((byte & (1 << (7 - i))) == 0) {
            return i;
        }
    }
    
    return -1;
}


// Scans the given bit array and returns the index to the first bit set. The
// bits in the array are numbered from 0 to nbits-1, with 0 being the first bit at 'pBits'.
// -1 is returned if no set bit is found.
Int Bits_FindFirstSet(const BitPointer pBits, Int nbits)
{
    if (nbits == 0) {
        return -1;
    }
    
    const BitPointer pLastBit = BitPointer_AddBitOffset(pBits, nbits - 1);
    
    if (pBits.bytePointer == pLastBit.bytePointer) {
        return index_of_first_1_in_byte(*pBits.bytePointer, pBits.bitIndex, pLastBit.bitIndex);
    }
    else {
        Byte* middle_byte_p = pBits.bytePointer + 1;
        const Int middle_byte_count = pLastBit.bytePointer - middle_byte_p;
        Int idx;
        
        // first byte
        idx = index_of_first_1_in_byte(*pBits.bytePointer, pBits.bitIndex, 7);
        if (idx != -1) { return idx; }
        
        // middle range
        if (middle_byte_count > 0) {
            Int byteOffset = Bytes_FindFirstNotEquals(middle_byte_p, middle_byte_count, 0);
            
            if (byteOffset != -1) {
                idx = index_of_first_1_in_byte(middle_byte_p[byteOffset], 0, 7);
                if (idx != -1) { return (7 - pBits.bitIndex) + 1 + (byteOffset << 3) + idx; }
            }
        }
        
        // last byte
        idx = index_of_first_1_in_byte(*pLastBit.bytePointer, 0, pLastBit.bitIndex);
        if (idx != -1) { return (7 - pBits.bitIndex) + 1 + (middle_byte_count << 3) + idx; }
        
        return -1;
    }
}

// Similar to Bits_FindFIrstSet() but scans from right to left.
Int Bits_FindLastSet(const BitPointer pBits, Int nbits)
{
    if (nbits == 0) {
        return -1;
    }
    
    const BitPointer pLastBit = BitPointer_AddBitOffset(pBits, nbits - 1);
    
    if (pBits.bytePointer == pLastBit.bytePointer) {
        return index_of_last_1_in_byte(*pBits.bytePointer, pLastBit.bitIndex, pBits.bitIndex);
    }
    else {
        Byte* middle_byte_p = pBits.bytePointer + 1;
        const Int middle_byte_count = pLastBit.bytePointer - middle_byte_p;
        Int idx;
        
        // last byte
        idx = index_of_last_1_in_byte(*pLastBit.bytePointer, pLastBit.bitIndex, 0);
        if (idx != -1) { return (7 - pBits.bitIndex) + 1 + (middle_byte_count << 3) + idx; }
        
        // middle range
        if (middle_byte_count > 0) {
            Int byteOffset = Bytes_FindLastNotEquals(middle_byte_p, middle_byte_count, 0);
            
            if (byteOffset != -1) {
                idx = index_of_last_1_in_byte(middle_byte_p[byteOffset], 7, 0);
                if (idx != -1) { return (7 - pBits.bitIndex) + 1 + (byteOffset << 3) + idx; }
            }
        }
        
        // first byte
        idx = index_of_last_1_in_byte(*pBits.bytePointer, 7, pBits.bitIndex);
        if (idx != -1) { return idx; }
        
        return -1;
    }
}

// Scans the given bit array and returns the index to the first bit cleared. The
// bits in the array are numbered from 0 to nbits-1, with 0 being the first bit at 'pBits'.
// -1 is returned if no set bit is found.
Int Bits_FindFirstCleared(const BitPointer pBits, Int nbits)
{
    if (nbits == 0) {
        return -1;
    }
    
    const BitPointer pLastBit = BitPointer_AddBitOffset(pBits, nbits - 1);
    
    if (pBits.bytePointer == pLastBit.bytePointer) {
        return index_of_first_0_in_byte(*pBits.bytePointer, pBits.bitIndex, pLastBit.bitIndex);
    }
    else {
        Byte* middle_byte_p = pBits.bytePointer + 1;
        const Int middle_byte_count = pLastBit.bytePointer - middle_byte_p;
        Int idx;
        
        // first byte
        idx = index_of_first_0_in_byte(*pBits.bytePointer, pBits.bitIndex, 7);
        if (idx != -1) { return idx; }
        
        // middle range
        if (middle_byte_count > 0) {
            Int byteOffset = Bytes_FindFirstNotEquals(middle_byte_p, middle_byte_count, 0xff);
            
            if (byteOffset != -1) {
                idx = index_of_first_0_in_byte(middle_byte_p[byteOffset], 0, 7);
                if (idx != -1) { return (7 - pBits.bitIndex) + 1 + (byteOffset << 3) + idx; }
            }
        }
        
        // last byte
        idx = index_of_first_0_in_byte(*pLastBit.bytePointer, 0, pLastBit.bitIndex);
        if (idx != -1) { return (7 - pBits.bitIndex) + 1 + (middle_byte_count << 3) + idx; }
        
        return -1;
    }
}

// Similar to Bits_FindFirstCleared() but scans from right to left.
Int Bits_FindLastCleared(const BitPointer pBits, Int nbits)
{
    if (nbits == 0) {
        return -1;
    }
    
    const BitPointer pLastBit = BitPointer_AddBitOffset(pBits, nbits - 1);
    
    if (pBits.bytePointer == pLastBit.bytePointer) {
        return index_of_last_0_in_byte(*pBits.bytePointer, pLastBit.bitIndex, pBits.bitIndex);
    }
    else {
        Byte* middle_byte_p = pBits.bytePointer + 1;
        const Int middle_byte_count = pLastBit.bytePointer - middle_byte_p;
        Int idx;
        
        // last byte
        idx = index_of_last_0_in_byte(*pLastBit.bytePointer, pLastBit.bitIndex, 0);
        if (idx != -1) { return (7 - pBits.bitIndex) + 1 + (middle_byte_count << 3) + idx; }
        
        // middle range
        if (middle_byte_count > 0) {
            Int byteOffset = Bytes_FindLastNotEquals(middle_byte_p, middle_byte_count, 0xff);
            
            if (byteOffset != -1) {
                idx = index_of_last_0_in_byte(middle_byte_p[byteOffset], 7, 0);
                if (idx != -1) { return (7 - pBits.bitIndex) + 1 + (byteOffset << 3) + idx; }
            }
        }
        
        // first byte
        idx = index_of_last_0_in_byte(*pBits.bytePointer, 7, pBits.bitIndex);
        if (idx != -1) { return idx; }
        
        return -1;
    }
}

// Sets 'nbits' bits starting at 'pBits'.
void Bits_SetRange(const BitPointer pBits, Int nbits)
{
    if (nbits == 0) {
        return;
    }
    
    const BitPointer pLastBit = BitPointer_AddBitOffset(pBits, nbits - 1);
    
    if (pBits.bytePointer == pLastBit.bytePointer) {
        for (Int i = pBits.bitIndex; i <= pLastBit.bitIndex; i++) {
            *pBits.bytePointer |= (1 << (7 - i));
        }
    } else {
        Byte* middle_start_p = pBits.bytePointer;
        Byte* middle_end_p = pLastBit.bytePointer;
        
        // first byte
        if (pBits.bitIndex > 0) {
            *pBits.bytePointer |= (0xffu >> pBits.bitIndex);
            middle_start_p++;
        }
        
        // last byte
        if (pLastBit.bitIndex < 7) {
            *pLastBit.bytePointer |= (0xffu << (7 - pLastBit.bitIndex));
        } else {
            middle_end_p++;
        }
        
        // middle range
        Bytes_SetRange(middle_start_p, middle_end_p - middle_start_p, 0xff);
    }
}

// Sets 'nbits' bits starting at 'pBits'.
void Bits_ClearRange(const BitPointer pBits, Int nbits)
{
    if (nbits == 0) {
        return;
    }
    
    const BitPointer pLastBit = BitPointer_AddBitOffset(pBits, nbits - 1);
    
    if (pBits.bytePointer == pLastBit.bytePointer) {
        for (Int i = pBits.bitIndex; i <= pLastBit.bitIndex; i++) {
            *pBits.bytePointer &= ~(1 << (7 - i));
        }
    } else {
        Byte* middle_start_p = pBits.bytePointer;
        Byte* middle_end_p = pLastBit.bytePointer;
        
        // first byte
        if (pBits.bitIndex > 0) {
            *pBits.bytePointer &= ~(0xffu >> pBits.bitIndex);
            middle_start_p++;
        }
        
        // last byte
        if (pLastBit.bitIndex < 7) {
            *pLastBit.bytePointer &= ~(0xffu << (7 - pLastBit.bitIndex));
        } else {
            middle_end_p++;
        }
        
        // middle range
        Bytes_ClearRange(middle_start_p, middle_end_p - middle_start_p);
    }
}

// Copies the bit range with length 'nbits' from 'pSrcBits' to 'pDstBits'.
void Bits_CopyRange(BitPointer pDstBits, const BitPointer pSrcBits, Int nbits)
{
    if (nbits == 0 || BitPointer_Equals(pDstBits, pSrcBits)) {
        return;
    }
    
    const BitPointer pSrcLastBit = BitPointer_AddBitOffset(pSrcBits, nbits - 1);
    const BitPointer pDstLastBit = BitPointer_AddBitOffset(pDstBits, nbits - 1);
    
    if (pSrcBits.bitIndex == pDstBits.bitIndex && nbits >= 8) {
        // The destination covers >= 1 byte and the start bit index for source and destination are the same.
        // This means that we can copy bytes 1:1 and we don't have to shift bits while copying.
        Byte src_first_byte = *pSrcBits.bytePointer;
        Byte src_last_byte = *pSrcLastBit.bytePointer;
        Byte dst_first_byte = *pDstBits.bytePointer;
        Byte dst_last_byte = *pDstLastBit.bytePointer;
        Byte* src_middle_start_p = pSrcBits.bytePointer;
        Byte* dst_middle_start_p = pDstBits.bytePointer;
        Byte* src_middle_end_p = pSrcLastBit.bytePointer;
        
        // first byte
        if (pSrcBits.bitIndex > 0) {
            Byte first_byte_mask = 0xffu >> pSrcBits.bitIndex;
            dst_first_byte &= ~first_byte_mask;
            dst_first_byte |= (src_first_byte & first_byte_mask);
            src_middle_start_p++; dst_middle_start_p++;
        }
        
        // last byte
        if (pSrcLastBit.bitIndex < 7) {
            Byte last_byte_mask = 0xffu << (7 - pSrcLastBit.bitIndex);
            dst_last_byte &= ~last_byte_mask;
            dst_last_byte |= (src_last_byte & last_byte_mask);
        } else {
            src_middle_end_p++;
        }
        
        // middle range
        Bytes_CopyRange(dst_middle_start_p, src_middle_start_p, src_middle_end_p - src_middle_start_p);
        
        // write the partial first & last bytes
        if (pSrcBits.bitIndex > 0) {
            *pDstBits.bytePointer = dst_first_byte;
        }
        if (pSrcLastBit.bitIndex < 7) {
            *pDstLastBit.bytePointer = dst_last_byte;
        }
    }
    else if (BitPointer_GreaterEquals(pDstBits, pSrcBits) && BitPointer_LessEquals(pDstBits, pSrcLastBit)) {
        // The destination covers > 1 byte and we need to shift bits while copying because the start
        // source and destination bit indexes are different. Source and destination ranges also
        // overlap.
        BitPointer pSrcPtr = pSrcLastBit;
        BitPointer pDstPtr = pDstLastBit;
        
        for (Int i = 0; i < nbits; i++) {
            Bits_Copy(pDstPtr, pSrcPtr);
            pSrcPtr = BitPointer_Decremented(pSrcPtr);
            pDstPtr = BitPointer_Decremented(pDstPtr);
        }
        
    }
    else {
        // The destination covers > 1 byte and we need to shift bits while copying because the start
        // source and destination bit indexes are different. Source and destination ranges do not
        // overlap.
        BitPointer pSrcPtr = pSrcBits;
        BitPointer pDstPtr = pDstBits;
        
        for (Int i = 0; i < nbits; i++) {
            Bits_Copy(pDstPtr, pSrcPtr);
            pSrcPtr = BitPointer_Incremented(pSrcPtr);
            pDstPtr = BitPointer_Incremented(pDstPtr);
        }
    }
}

// Prints the given bit array
#define test_bit_in_byte(bnum, byte)   ((byte) & (1 << (7 - (bnum))))
void Bits_Print(const BitPointer pBits, Int nbits)
{
    for (Int i = 0; i < nbits >> 3; i++) {
        register Byte byte = pBits.bytePointer[i];
        
        for (Int j = 0; j < 8; j++) {
            register Int bit = test_bit_in_byte(j, byte);
            
            print((bit) ? "1" : "0");
        }
        
        print(" ");
    }
    
    print("\n");
}
