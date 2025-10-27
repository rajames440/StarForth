/*
    Title:  Bitmap.  Generally used by the garbage collector to indicate allocated words

    Copyright (c) 2006, 2012, 2017  David C.J. Matthews
       Based on original code in garbage_collect.c.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.
    
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.
    
    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

*/

/*
   Bitmaps are used particularly in the garbage collector to indicate allocated
   words.  The efficiency of this code is crucial for the speed of the garbage
   collector.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#elif defined(_WIN32)
#include "winconfig.h"
#else
#error "No configuration file"
#endif

#ifdef HAVE_ASSERT_H
#include <assert.h>
#define ASSERT(h) assert(h)
#else
#define ASSERT(h)
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "bitmap.h"
#include "globals.h"

bool Bitmap::Create(size_t bits)
{
    free(m_bits); // Any previous data
    size_t bytes = (bits+7) >> 3;
    m_bits = (unsigned char*)calloc(bytes, sizeof(unsigned char));
    return m_bits != 0;
}

void Bitmap::Destroy()
{
    free(m_bits);
    m_bits = 0;
}

Bitmap::~Bitmap()
{
    Destroy();
}

// Set a range of bits in a bitmap.  This checks that the bits are not already set.
void Bitmap::SetBits(uintptr_t bitno, uintptr_t length)
{
    uintptr_t byte_index = bitno >> 3;
    
    ASSERT (0 < length); // Strictly positive
    
    /* Set the first part byte */
    uintptr_t start_bit_index = bitno & 7;
    uintptr_t stop_bit_index  = start_bit_index + length;
    /* Do we need to change more than one byte? */
    if (stop_bit_index < 8)
    {
        const unsigned mask1 = 0xff << start_bit_index;
        const unsigned mask2 = 0xff << stop_bit_index;
        const unsigned mask  = mask1 & ~mask2;
        
//        ASSERT((m_bits[byte_index] & mask) == 0);
        m_bits[byte_index] |= mask;
        return;
    }
    else /* Set all the bits we can in the first byte */
    {
        const unsigned mask  = 0xff << start_bit_index;
        
//        ASSERT((m_bits[byte_index] & mask) == 0);
        m_bits[byte_index] |= mask;
        
        /* length = length - (8 - start_bit_index); */
        length = stop_bit_index - 8;
    }
    
    /* Set as many full bytes as possible */
    if (8 <= length)
    {
        memset(m_bits + byte_index + 1, 0xff, length >> 3);
        byte_index += length >> 3;
        length &= 7;
    }
    
    /* Invariant: 0 <= length < 8 */
    ASSERT(length < 8);
    if (length == 0) return;
    
    /* Invariant: 0 < length < 8 */
    
    /* Set the final part byte */
    byte_index ++;
    const unsigned mask  = 0xff & ~(0xff << length);
//    ASSERT((m_bits[byte_index] & mask) == 0);
    m_bits[byte_index] |= mask;
}

// This previously cleared whole bytes.  It now works the same as
// SetBits to clear a range of bits and is a modified version of that code.
void Bitmap::ClearBits(uintptr_t bitno, uintptr_t length)
{
    uintptr_t byte_index = bitno >> 3;
    uintptr_t start_bit_index = bitno & 7;
    uintptr_t stop_bit_index = start_bit_index + length;
    /* Do we need to change more than one byte? */
    if (stop_bit_index < 8)
    {
        const unsigned mask1 = 0xff << start_bit_index;
        const unsigned mask2 = 0xff << stop_bit_index;
        const unsigned mask = mask1 & ~mask2;
        m_bits[byte_index] &= 0xff ^ mask;
        return;
    }
    else /* Clear all the bits we can in the first byte */
    {
        const unsigned mask = 0xff << start_bit_index;
        m_bits[byte_index] &= 0xff ^ mask;
        length = stop_bit_index - 8;
    }

    /* Clear as many full bytes as possible */
    if (8 <= length)
    {
        memset(m_bits + byte_index + 1, 0, length >> 3);
        byte_index += length >> 3;
        length &= 7;
    }

    /* Invariant: 0 <= length < 8 */
    ASSERT(length < 8);
    if (length == 0) return;

    /* Invariant: 0 < length < 8 */

    /* Clear the final part byte */
    byte_index++;
    const unsigned mask = 0xff & ~(0xff << length);
    //    ASSERT((m_bits[byte_index] & mask) == 0);
    m_bits[byte_index] &= 0xff ^ mask;

}

// How many zero bits (maximum n) are there in the bitmap, starting at location start? */
uintptr_t Bitmap::CountZeroBits(uintptr_t bitno, uintptr_t n) const
{
    uintptr_t byte_index = bitno >> 3;
    unsigned bit_index  = bitno & 7;
    unsigned mask  = 1 << bit_index;
    uintptr_t zero_bits  = 0;
    ASSERT (0 < n); // Strictly positive
    
    /* Check the first part byte */
    while (mask != 0)
    {
        /* zero_bits < n */
        if ((m_bits[byte_index] & mask) != 0) return zero_bits;
        zero_bits ++;
        if (zero_bits == n) return zero_bits;
        mask = (mask << 1) & 0xff;
        /* zero_bits < n */
    }
    
    /* zero_bits < n */
    
    /* Check as many bytes as possible */
    byte_index ++;
    while (zero_bits < n && m_bits[byte_index] == 0)
    {
        zero_bits += 8;
        byte_index ++;
    }
    
    /* Check the final part byte */
    mask = 1;
    while (zero_bits < n && (m_bits[byte_index] & mask) == 0)
    {
        zero_bits ++;
        mask = (mask << 1) & 0xff;
    }
    
    return zero_bits;
}


// Search the bitmap from the high end down looking for n contiguous zeros
// Returns the value of "bitno" on failure. .
uintptr_t Bitmap::FindFree
(
  uintptr_t   limit,  /* The highest numbered bit that's too small to use */
  uintptr_t   start,  /* The lowest numbered bit that's too large to use */
  uintptr_t   n       /* The number of consecutive zero bits required */
) const
{
    if (limit + n >= start)
        return start; // Failure

    uintptr_t candidate = start - n;
    ASSERT (start > limit);
    
    while (1)
    {
        uintptr_t bits_free = CountZeroBits(candidate, n);
        
        if (n <= bits_free)
            return candidate;

        if (candidate < n - bits_free + limit)
            return start; // Failure
        
        candidate -= (n - bits_free);
    }
}

// Count the number of set bits in the bitmap.
uintptr_t Bitmap::CountSetBits(uintptr_t size) const
{
    size_t bytes = (size+7) >> 3;
    uintptr_t count = 0;
    for (size_t i = 0; i < bytes; i++)
    {
        unsigned char byte = m_bits[i];
        if (byte == 0xff) // Common case
            count += 8;
        else
        {
            while (byte != 0)
            {
                unsigned char b = byte & (-byte);
                count++;
                byte -= b;
            }
        }
    }
    return count;
}

// Find the last set bit before here.  Used to find the start of a code cell.
// Returns zero if no bit is set.
uintptr_t Bitmap::FindLastSet(uintptr_t bitno) const
{
    size_t byteno = bitno >> 3;
    // Code cells are quite long so most of the bitmap will be zero.
    if (m_bits[byteno] == 0)
    {
       do {
            if (byteno == 0) return 0;
            byteno--;
        } while (m_bits[byteno] == 0);
        bitno = (byteno << 3) + 7; // Set it to the last bit
    }
    while (bitno > 0 && ! TestBit(bitno)) bitno--;
    return bitno;
}