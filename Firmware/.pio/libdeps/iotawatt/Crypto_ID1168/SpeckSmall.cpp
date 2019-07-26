/*
 * Copyright (C) 2016 Southern Storm Software, Pty Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "SpeckSmall.h"
#include "Crypto.h"
#include "utility/RotateUtil.h"
#include "utility/EndianUtil.h"
#include <string.h>

/**
 * \class SpeckSmall SpeckSmall.h <SpeckSmall.h>
 * \brief Speck block cipher with a 128-bit block size (small-memory version).
 *
 * This class differs from the Speck class in that the RAM requirements are
 * vastly reduced.  The key schedule is expanded round by round instead of
 * being generated and stored by setKey().  The performance of encryption
 * and decryption is slightly less because of this.
 *
 * This class is useful when RAM is at a premium and reduced encryption
 * performance is not a hindrance to the application.  Even though the
 * performance is reduced, this class is still faster than AES with
 * equivalent key sizes.
 *
 * The companion SpeckTiny class uses even less RAM but only supports the
 * encryptBlock() operation.  Block cipher modes like CTR, EAX, and GCM
 * do not need the decryptBlock() operation, so SpeckTiny may be a better
 * option than SpeckSmall for many applications.
 *
 * See the documentation for the Speck class for more information on the
 * Speck family of block ciphers.
 *
 * References: https://en.wikipedia.org/wiki/Speck_%28cipher%29,
 * http://eprint.iacr.org/2013/404
 *
 * \sa Speck, SpeckTiny
 */

// The "avr-gcc" compiler doesn't do a very good job of compiling
// code involving 64-bit values.  So we have to use inline assembly.
// It also helps to break the state up into 32-bit quantities
// because "asm" supports register names like %A0, %B0, %C0, %D0
// for the bytes in a 32-bit quantity, but it does not support
// %E0, %F0, %G0, %H0 for the high bytes of a 64-bit quantity.
#if defined(__AVR__)
#define USE_AVR_INLINE_ASM 1
#endif

// Pack/unpack byte-aligned big-endian 64-bit quantities.
#define pack64(data, value) \
    do { \
        uint64_t v = htobe64((value)); \
        memcpy((data), &v, sizeof(uint64_t)); \
    } while (0)
#define unpack64(value, data) \
    do { \
        memcpy(&(value), (data), sizeof(uint64_t)); \
        (value) = be64toh((value)); \
    } while (0)

/**
 * \brief Constructs a small-memory Speck block cipher with no initial key.
 *
 * This constructor must be followed by a call to setKey() before the
 * block cipher can be used for encryption or decryption.
 */
SpeckSmall::SpeckSmall()
{
}

SpeckSmall::~SpeckSmall()
{
    clean(l);
}

bool SpeckSmall::setKey(const uint8_t *key, size_t len)
{
    // Try setting the key for the forward encryption direction.
    if (!SpeckTiny::setKey(key, len))
        return false;

#if USE_AVR_INLINE_ASM
    // Expand the key schedule to get the l and s values at the end
    // of the schedule, which will allow us to reverse it later.
    uint8_t mb = (rounds - 31) * 8;
    __asm__ __volatile__ (
        "ld r16,Z+\n"               // s = k[0]
        "ld r17,Z+\n" 
        "ld r18,Z+\n" 
        "ld r19,Z+\n" 
        "ld r20,Z+\n" 
        "ld r21,Z+\n" 
        "ld r22,Z+\n" 
        "ld r23,Z+\n" 

        "mov r24,%3\n"              // memcpy(l, k + 1, mb)
        "3:\n"
        "ld __tmp_reg__,Z+\n"
        "st X+,__tmp_reg__\n"
        "dec r24\n"
        "brne 3b\n"
        "sub %A1,%3\n"              // return X to its initial value
        "sbc %B1,__zero_reg__\n"

        "1:\n"

        // l[li_out] = (s + rightRotate8_64(l[li_in])) ^ i;
        "add %A1,%2\n"              // X = &(l[li_in])
        "adc %B1,__zero_reg__\n"
        "ld r15,X+\n"               // x = rightRotate8_64(l[li_in])
        "ld r8,X+\n"
        "ld r9,X+\n"
        "ld r10,X+\n"
        "ld r11,X+\n"
        "ld r12,X+\n"
        "ld r13,X+\n"
        "ld r14,X+\n"

        "add r8,r16\n"              // x += s
        "adc r9,r17\n"
        "adc r10,r18\n"
        "adc r11,r19\n"
        "adc r12,r20\n"
        "adc r13,r21\n"
        "adc r14,r22\n"
        "adc r15,r23\n"

        "eor r8,%4\n"               // x ^= i

        // X = X - li_in + li_out
        "ldi r24,8\n"               // li_in = li_in + 1
        "add %2,r24\n"
        "sub %A1,%2\n"              // return X to its initial value
        "sbc %B1,__zero_reg__\n"
        "ldi r25,0x1f\n"
        "and %2,r25\n"              // li_in = li_in % 4
        "add %A1,%3\n"              // X = &(l[li_out])
        "adc %B1,__zero_reg__\n"

        "st X+,r8\n"                // l[li_out] = x
        "st X+,r9\n"
        "st X+,r10\n"
        "st X+,r11\n"
        "st X+,r12\n"
        "st X+,r13\n"
        "st X+,r14\n"
        "st X+,r15\n"

        "add %3,r24\n"              // li_out = li_out + 1
        "sub %A1,%3\n"              // return X to its initial value
        "sbc %B1,__zero_reg__\n"
        "and %3,r25\n"              // li_out = li_out % 4

        // s = leftRotate3_64(s) ^ l[li_out];
        "lsl r16\n"                 // s = leftRotate1_64(s)
        "rol r17\n"
        "rol r18\n"
        "rol r19\n"
        "rol r20\n"
        "rol r21\n"
        "rol r22\n"
        "rol r23\n"
        "adc r16,__zero_reg__\n"

        "lsl r16\n"                 // s = leftRotate1_64(s)
        "rol r17\n"
        "rol r18\n"
        "rol r19\n"
        "rol r20\n"
        "rol r21\n"
        "rol r22\n"
        "rol r23\n"
        "adc r16,__zero_reg__\n"

        "lsl r16\n"                 // s = leftRotate1_64(s)
        "rol r17\n"
        "rol r18\n"
        "rol r19\n"
        "rol r20\n"
        "rol r21\n"
        "rol r22\n"
        "rol r23\n"
        "adc r16,__zero_reg__\n"

        "eor r16,r8\n"              // s ^= x
        "eor r17,r9\n"
        "eor r18,r10\n"
        "eor r19,r11\n"
        "eor r20,r12\n"
        "eor r21,r13\n"
        "eor r22,r14\n"
        "eor r23,r15\n"

        // Loop
        "inc %4\n"                  // ++i
        "dec %5\n"                  // --rounds
        "breq 2f\n"
        "rjmp 1b\n"
        "2:\n"

        "add %A1,%3\n"              // X = &(l[li_out])
        "adc %B1,__zero_reg__\n"
        "st X+,r16\n"               // l[li_out] = s
        "st X+,r17\n"
        "st X+,r18\n"
        "st X+,r19\n"
        "st X+,r20\n"
        "st X+,r21\n"
        "st X+,r22\n"
        "st X+,r23\n"

        : : "z"(k), "x"(l),
            "r"((uint8_t)0),                // initial value of li_in
            "r"((uint8_t)mb),               // initial value of li_out
            "r"(0),                         // initial value of i
            "r"(rounds - 1)
        :  "r8",  "r9", "r10", "r11", "r12", "r13", "r14", "r15",
          "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
          "r24", "r25"
    );
    return true;
#else
    // Expand the key schedule to get the l and s values at the end
    // of the schedule, which will allow us to reverse it later.
    uint8_t m = rounds - 30;
    uint8_t li_in = 0;
    uint8_t li_out = m - 1;
    uint64_t s = k[0];
    memcpy(l, k + 1, (m - 1) * sizeof(uint64_t));
    for (uint8_t i = 0; i < (rounds - 1); ++i) {
        l[li_out] = (s + rightRotate8_64(l[li_in])) ^ i;
        s = leftRotate3_64(s) ^ l[li_out];
        li_in = (li_in + 1) & 0x03;
        li_out = (li_out + 1) & 0x03;
    }

    // Save the final s value in the l array so that we can recover it later.
    l[li_out] = s;
    return true;
#endif
}

void SpeckSmall::decryptBlock(uint8_t *output, const uint8_t *input)
{
#if USE_AVR_INLINE_ASM
    // Automatically generated by the genspeck tool.
    uint64_t l[5];
    uint8_t r = rounds;
    uint8_t li_in = ((r + 3) & 0x03) * 8;
    uint8_t li_out = ((((r - 31) & 0x03) * 8) + li_in) & 0x1F;
    __asm__ __volatile__ (
        "ldd r25,%4\n"
        "ldi r24,32\n"
        "1:\n"
        "ld __tmp_reg__,X+\n"
        "st Z+,__tmp_reg__\n"
        "dec r24\n"
        "brne 1b\n"
        "movw r26,r30\n"
        "sbiw r30,32\n"
        "add r30,r25\n"
        "adc r31,__zero_reg__\n"
        "ld __tmp_reg__,Z\n"
        "st X+,__tmp_reg__\n"
        "ldd __tmp_reg__,Z+1\n"
        "st X+,__tmp_reg__\n"
        "ldd __tmp_reg__,Z+2\n"
        "st X+,__tmp_reg__\n"
        "ldd __tmp_reg__,Z+3\n"
        "st X+,__tmp_reg__\n"
        "ldd __tmp_reg__,Z+4\n"
        "st X+,__tmp_reg__\n"
        "ldd __tmp_reg__,Z+5\n"
        "st X+,__tmp_reg__\n"
        "ldd __tmp_reg__,Z+6\n"
        "st X+,__tmp_reg__\n"
        "ldd __tmp_reg__,Z+7\n"
        "st X+,__tmp_reg__\n"
        "sub r30,r25\n"
        "sbc r31,__zero_reg__\n"
        "movw r26,%A2\n"
        "ld r15,X+\n"
        "ld r14,X+\n"
        "ld r13,X+\n"
        "ld r12,X+\n"
        "ld r11,X+\n"
        "ld r10,X+\n"
        "ld r9,X+\n"
        "ld r8,X+\n"
        "ld r23,X+\n"
        "ld r22,X+\n"
        "ld r21,X+\n"
        "ld r20,X+\n"
        "ld r19,X+\n"
        "ld r18,X+\n"
        "ld r17,X+\n"
        "ld r16,X\n"
        "ldd %A2,%6\n"
        "mov %B2,r25\n"
        "ldd r25,%5\n"
        "dec r25\n"
        "movw r26,r30\n"
        "adiw r26,40\n"
        "2:\n"
        "eor r16,r8\n"
        "eor r17,r9\n"
        "eor r18,r10\n"
        "eor r19,r11\n"
        "eor r20,r12\n"
        "eor r21,r13\n"
        "eor r22,r14\n"
        "eor r23,r15\n"
        "bst r16,0\n"
        "ror r23\n"
        "ror r22\n"
        "ror r21\n"
        "ror r20\n"
        "ror r19\n"
        "ror r18\n"
        "ror r17\n"
        "ror r16\n"
        "bld r23,7\n"
        "bst r16,0\n"
        "ror r23\n"
        "ror r22\n"
        "ror r21\n"
        "ror r20\n"
        "ror r19\n"
        "ror r18\n"
        "ror r17\n"
        "ror r16\n"
        "bld r23,7\n"
        "bst r16,0\n"
        "ror r23\n"
        "ror r22\n"
        "ror r21\n"
        "ror r20\n"
        "ror r19\n"
        "ror r18\n"
        "ror r17\n"
        "ror r16\n"
        "bld r23,7\n"
        "ld __tmp_reg__,-X\n"
        "eor __tmp_reg__,r15\n"
        "ld r15,-X\n"
        "eor r15,r14\n"
        "ld r14,-X\n"
        "eor r14,r13\n"
        "ld r13,-X\n"
        "eor r13,r12\n"
        "ld r12,-X\n"
        "eor r12,r11\n"
        "ld r11,-X\n"
        "eor r11,r10\n"
        "ld r10,-X\n"
        "eor r10,r9\n"
        "ld r9,-X\n"
        "eor r9,r8\n"
        "mov r8,__tmp_reg__\n"
        "sub r9,r16\n"
        "sbc r10,r17\n"
        "sbc r11,r18\n"
        "sbc r12,r19\n"
        "sbc r13,r20\n"
        "sbc r14,r21\n"
        "sbc r15,r22\n"
        "sbc r8,r23\n"
        "or r25,r25\n"
        "brne 3f\n"
        "rjmp 4f\n"
        "3:\n"
        "dec r25\n"
        "push r8\n"
        "push r9\n"
        "push r10\n"
        "push r11\n"
        "push r12\n"
        "push r13\n"
        "push r14\n"
        "push r15\n"
        "push r16\n"
        "push r17\n"
        "push r18\n"
        "push r19\n"
        "push r20\n"
        "push r21\n"
        "push r22\n"
        "push r23\n"
        "ldi r24,24\n"
        "add %A2,r24\n"
        "add %B2,r24\n"
        "ldi r24,0x1F\n"
        "and %A2,r24\n"
        "and %B2,r24\n"
        "ld r16,X+\n"
        "ld r17,X+\n"
        "ld r18,X+\n"
        "ld r19,X+\n"
        "ld r20,X+\n"
        "ld r21,X+\n"
        "ld r22,X+\n"
        "ld r23,X+\n"
        "add r30,%B2\n"
        "adc r31,__zero_reg__\n"
        "ld r8,Z\n"
        "ldd r9,Z+1\n"
        "ldd r10,Z+2\n"
        "ldd r11,Z+3\n"
        "ldd r12,Z+4\n"
        "ldd r13,Z+5\n"
        "ldd r14,Z+6\n"
        "ldd r15,Z+7\n"
        "sub r30,%B2\n"
        "sbc r31,__zero_reg__\n"
        "eor r16,r8\n"
        "eor r17,r9\n"
        "eor r18,r10\n"
        "eor r19,r11\n"
        "eor r20,r12\n"
        "eor r21,r13\n"
        "eor r22,r14\n"
        "eor r23,r15\n"
        "bst r16,0\n"
        "ror r23\n"
        "ror r22\n"
        "ror r21\n"
        "ror r20\n"
        "ror r19\n"
        "ror r18\n"
        "ror r17\n"
        "ror r16\n"
        "bld r23,7\n"
        "bst r16,0\n"
        "ror r23\n"
        "ror r22\n"
        "ror r21\n"
        "ror r20\n"
        "ror r19\n"
        "ror r18\n"
        "ror r17\n"
        "ror r16\n"
        "bld r23,7\n"
        "bst r16,0\n"
        "ror r23\n"
        "ror r22\n"
        "ror r21\n"
        "ror r20\n"
        "ror r19\n"
        "ror r18\n"
        "ror r17\n"
        "ror r16\n"
        "bld r23,7\n"
        "st -X,r23\n"
        "st -X,r22\n"
        "st -X,r21\n"
        "st -X,r20\n"
        "st -X,r19\n"
        "st -X,r18\n"
        "st -X,r17\n"
        "st -X,r16\n"
        "adiw r26,8\n"
        "eor r8,r25\n"
        "sub r8,r16\n"
        "sbc r9,r17\n"
        "sbc r10,r18\n"
        "sbc r11,r19\n"
        "sbc r12,r20\n"
        "sbc r13,r21\n"
        "sbc r14,r22\n"
        "sbc r15,r23\n"
        "add r30,%A2\n"
        "adc r31,__zero_reg__\n"
        "st Z,r15\n"
        "std Z+1,r8\n"
        "std Z+2,r9\n"
        "std Z+3,r10\n"
        "std Z+4,r11\n"
        "std Z+5,r12\n"
        "std Z+6,r13\n"
        "std Z+7,r14\n"
        "sub r30,%A2\n"
        "sbc r31,__zero_reg__\n"
        "pop r23\n"
        "pop r22\n"
        "pop r21\n"
        "pop r20\n"
        "pop r19\n"
        "pop r18\n"
        "pop r17\n"
        "pop r16\n"
        "pop r15\n"
        "pop r14\n"
        "pop r13\n"
        "pop r12\n"
        "pop r11\n"
        "pop r10\n"
        "pop r9\n"
        "pop r8\n"
        "rjmp 2b\n"
        "4:\n"
        "ldd r26,%A3\n"
        "ldd r27,%B3\n"
        "st X+,r15\n"
        "st X+,r14\n"
        "st X+,r13\n"
        "st X+,r12\n"
        "st X+,r11\n"
        "st X+,r10\n"
        "st X+,r9\n"
        "st X+,r8\n"
        "st X+,r23\n"
        "st X+,r22\n"
        "st X+,r21\n"
        "st X+,r20\n"
        "st X+,r19\n"
        "st X+,r18\n"
        "st X+,r17\n"
        "st X,r16\n"
        : : "x"(this->l), "z"(l), "r"(input), "Q"(output), "Q"(li_out), "Q"(r), "Q"(li_in)
        : "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
          "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23", "memory"
        , "r24", "r25"
    );
#else
    uint64_t l[4];
    uint64_t x, y, s;
    uint8_t round;
    uint8_t li_in = (rounds + 3) & 0x03;
    uint8_t li_out = ((rounds - 31) + li_in) & 0x03;

    // Prepare the key schedule, starting at the end.
    for (round = li_in; round != li_out; round = (round + 1) & 0x03)
        l[round] = this->l[round];
    s = this->l[li_out];

    // Unpack the input and convert from big-endian.
    unpack64(x, input);
    unpack64(y, input + 8);

    // Perform all decryption rounds except the last while
    // expanding the decryption schedule on the fly.
    for (uint8_t round = rounds - 1; round > 0; --round) {
        // Decrypt using the current round key.
        y = rightRotate3_64(x ^ y);
        x = leftRotate8_64((x ^ s) - y);

        // Generate the round key for the previous round.
        li_in = (li_in + 3) & 0x03;
        li_out = (li_out + 3) & 0x03;
        s = rightRotate3_64(s ^ l[li_out]);
        l[li_in] = leftRotate8_64((l[li_out] ^ (round - 1)) - s);
    }

    // Perform the final decryption round.
    y = rightRotate3_64(x ^ y);
    x = leftRotate8_64((x ^ s) - y);

    // Pack the output and convert to big-endian.
    pack64(output, x);
    pack64(output + 8, y);
#endif
}

void SpeckSmall::clear()
{
    SpeckTiny::clear();
    clean(l);
}
