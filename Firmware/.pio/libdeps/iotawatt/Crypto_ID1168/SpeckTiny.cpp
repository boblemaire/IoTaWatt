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

#include "SpeckTiny.h"
#include "Crypto.h"
#include "utility/RotateUtil.h"
#include "utility/EndianUtil.h"
#include <string.h>

/**
 * \class SpeckTiny SpeckTiny.h <SpeckTiny.h>
 * \brief Speck block cipher with a 128-bit block size (tiny-memory version).
 *
 * This class differs from the Speck class in the following ways:
 *
 * \li RAM requirements are vastly reduced.  The key (up to 256 bits) is
 * stored directly and then expanded to the full key schedule round by round.
 * The setKey() method is very fast because of this.
 * \li Performance of encryptBlock() is slower than for Speck due to
 * expanding the key on the fly rather than ahead of time.
 * \li The decryptBlock() function is not supported, which means that CBC
 * mode cannot be used but the CTR, CFB, OFB, EAX, and GCM modes can be used.
 *
 * This class is useful when RAM is at a premium, CBC mode is not required,
 * and reduced encryption performance is not a hindrance to the application.
 * Even though the performance of encryptBlock() is reduced, this class is
 * still faster than AES with equivalent key sizes.
 *
 * The companion SpeckSmall class supports decryptBlock() at the cost of
 * some additional memory and slower setKey() times.
 *
 * See the documentation for the Speck class for more information on the
 * Speck family of block ciphers.
 *
 * References: https://en.wikipedia.org/wiki/Speck_%28cipher%29,
 * http://eprint.iacr.org/2013/404
 *
 * \sa Speck, SpeckSmall
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

/**
 * \brief Constructs a tiny-memory Speck block cipher with no initial key.
 *
 * This constructor must be followed by a call to setKey() before the
 * block cipher can be used for encryption.
 */
SpeckTiny::SpeckTiny()
    : rounds(32)
{
}

SpeckTiny::~SpeckTiny()
{
    clean(k);
}

size_t SpeckTiny::blockSize() const
{
    return 16;
}

size_t SpeckTiny::keySize() const
{
    // Also supports 128-bit and 192-bit, but we only report 256-bit.
    return 32;
}

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

bool SpeckTiny::setKey(const uint8_t *key, size_t len)
{
#if USE_AVR_INLINE_ASM
    // Determine the number of rounds to use and validate the key length.
    if (len == 32) {
        rounds = 34;
    } else if (len == 24) {
        rounds = 33;
    } else if (len == 16) {
        rounds = 32;
    } else {
        return false;
    }

    // Copy the bytes of the key into the "k" array in reverse order to
    // convert big endian into little-endian.
    __asm__ __volatile__ (
        "1:\n"
        "ld __tmp_reg__,-Z\n"
        "st X+,__tmp_reg__\n"
        "dec %2\n"
        "brne 1b\n"
        : : "x"(k), "z"(key + len), "r"(len)
    );
#else
    if (len == 32) {
        rounds = 34;
        unpack64(k[3], key);
        unpack64(k[2], key + 8);
        unpack64(k[1], key + 16);
        unpack64(k[0], key + 24);
    } else if (len == 24) {
        rounds = 33;
        unpack64(k[2], key);
        unpack64(k[1], key + 8);
        unpack64(k[0], key + 16);
    } else if (len == 16) {
        rounds = 32;
        unpack64(k[1], key);
        unpack64(k[0], key + 8);
    } else {
        return false;
    }
#endif
    return true;
}

void SpeckTiny::encryptBlock(uint8_t *output, const uint8_t *input)
{
#if USE_AVR_INLINE_ASM
    // Automatically generated by the genspeck tool.
    uint64_t l[5];
    uint8_t r = rounds;
    uint8_t mb = (r - 31) * 8;
    __asm__ __volatile__ (
        "movw r8,r30\n"
        "ldd r16,%4\n"
        "ldi r24,8\n"
        "add r16,r24\n"
        "1:\n"
        "ld __tmp_reg__,X+\n"
        "st Z+,__tmp_reg__\n"
        "dec r16\n"
        "brne 1b\n"
        "movw r30,r8\n"
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
        "clr %A2\n"
        "ldd %B2,%4\n"
        "clr r25\n"
        "2:\n"
        "add r9,r16\n"
        "adc r10,r17\n"
        "adc r11,r18\n"
        "adc r12,r19\n"
        "adc r13,r20\n"
        "adc r14,r21\n"
        "adc r15,r22\n"
        "adc r8,r23\n"
        "ld __tmp_reg__,Z+\n"
        "eor __tmp_reg__,r9\n"
        "ld r9,Z+\n"
        "eor r9,r10\n"
        "ld r10,Z+\n"
        "eor r10,r11\n"
        "ld r11,Z+\n"
        "eor r11,r12\n"
        "ld r12,Z+\n"
        "eor r12,r13\n"
        "ld r13,Z+\n"
        "eor r13,r14\n"
        "ld r14,Z+\n"
        "eor r14,r15\n"
        "ld r15,Z+\n"
        "eor r15,r8\n"
        "mov r8,__tmp_reg__\n"
        "lsl r16\n"
        "rol r17\n"
        "rol r18\n"
        "rol r19\n"
        "rol r20\n"
        "rol r21\n"
        "rol r22\n"
        "rol r23\n"
        "adc r16, __zero_reg__\n"
        "lsl r16\n"
        "rol r17\n"
        "rol r18\n"
        "rol r19\n"
        "rol r20\n"
        "rol r21\n"
        "rol r22\n"
        "rol r23\n"
        "adc r16, __zero_reg__\n"
        "lsl r16\n"
        "rol r17\n"
        "rol r18\n"
        "rol r19\n"
        "rol r20\n"
        "rol r21\n"
        "rol r22\n"
        "rol r23\n"
        "adc r16, __zero_reg__\n"
        "eor r16,r8\n"
        "eor r17,r9\n"
        "eor r18,r10\n"
        "eor r19,r11\n"
        "eor r20,r12\n"
        "eor r21,r13\n"
        "eor r22,r14\n"
        "eor r23,r15\n"
        "mov __tmp_reg__,r25\n"
        "inc __tmp_reg__\n"
        "ldd r24,%5\n"
        "cp __tmp_reg__,r24\n"
        "brne 3f\n"
        "rjmp 4f\n"
        "3:\n"
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
        "sbiw r30,8\n"
        "ld r16,Z\n"
        "ldd r17,Z+1\n"
        "ldd r18,Z+2\n"
        "ldd r19,Z+3\n"
        "ldd r20,Z+4\n"
        "ldd r21,Z+5\n"
        "ldd r22,Z+6\n"
        "ldd r23,Z+7\n"
        "add r30,%A2\n"
        "adc r31,__zero_reg__\n"
        "ldd r15,Z+8\n"
        "ldd r8,Z+9\n"
        "ldd r9,Z+10\n"
        "ldd r10,Z+11\n"
        "ldd r11,Z+12\n"
        "ldd r12,Z+13\n"
        "ldd r13,Z+14\n"
        "ldd r14,Z+15\n"
        "add r8,r16\n"
        "adc r9,r17\n"
        "adc r10,r18\n"
        "adc r11,r19\n"
        "adc r12,r20\n"
        "adc r13,r21\n"
        "adc r14,r22\n"
        "adc r15,r23\n"
        "eor r8,r25\n"
        "sub r30,%A2\n"
        "sbc r31,__zero_reg__\n"
        "add r30,%B2\n"
        "adc r31,__zero_reg__\n"
        "std Z+8,r8\n"
        "std Z+9,r9\n"
        "std Z+10,r10\n"
        "std Z+11,r11\n"
        "std Z+12,r12\n"
        "std Z+13,r13\n"
        "std Z+14,r14\n"
        "std Z+15,r15\n"
        "sub r30,%B2\n"
        "sbc r31,__zero_reg__\n"
        "lsl r16\n"
        "rol r17\n"
        "rol r18\n"
        "rol r19\n"
        "rol r20\n"
        "rol r21\n"
        "rol r22\n"
        "rol r23\n"
        "adc r16, __zero_reg__\n"
        "lsl r16\n"
        "rol r17\n"
        "rol r18\n"
        "rol r19\n"
        "rol r20\n"
        "rol r21\n"
        "rol r22\n"
        "rol r23\n"
        "adc r16, __zero_reg__\n"
        "lsl r16\n"
        "rol r17\n"
        "rol r18\n"
        "rol r19\n"
        "rol r20\n"
        "rol r21\n"
        "rol r22\n"
        "rol r23\n"
        "adc r16, __zero_reg__\n"
        "eor r16,r8\n"
        "eor r17,r9\n"
        "eor r18,r10\n"
        "eor r19,r11\n"
        "eor r20,r12\n"
        "eor r21,r13\n"
        "eor r22,r14\n"
        "eor r23,r15\n"
        "st Z,r16\n"
        "std Z+1,r17\n"
        "std Z+2,r18\n"
        "std Z+3,r19\n"
        "std Z+4,r20\n"
        "std Z+5,r21\n"
        "std Z+6,r22\n"
        "std Z+7,r23\n"
        "ldi r24,8\n"
        "add %A2,r24\n"
        "add %B2,r24\n"
        "ldi r24,0x1F\n"
        "and %A2,r24\n"
        "and %B2,r24\n"
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
        "inc r25\n"
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
        : : "x"(k), "z"(l), "r"(input), "Q"(output), "Q"(mb), "Q"(r)
        : "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
          "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23", "memory"
        , "r24", "r25"
    );
#else
    uint64_t l[4];
    uint64_t x, y, s;
    uint8_t round;
    uint8_t li_in = 0;
    uint8_t li_out = rounds - 31;
    uint8_t i = 0;

    // Copy the input block into the work registers.
    unpack64(x, input);
    unpack64(y, input + 8);

    // Prepare the key schedule.
    memcpy(l, k + 1, li_out * sizeof(uint64_t));
    s = k[0];

    // Perform all encryption rounds except the last.
    for (round = rounds - 1; round > 0; --round, ++i) {
        // Perform the round with the current key schedule word.
        x = (rightRotate8_64(x) + y) ^ s;
        y = leftRotate3_64(y) ^ x;

        // Calculate the next key schedule word.
        l[li_out] = (s + rightRotate8_64(l[li_in])) ^ i;
        s = leftRotate3_64(s) ^ l[li_out];
        li_in = (li_in + 1) & 0x03;
        li_out = (li_out + 1) & 0x03;
    }

    // Perform the final round and copy to the output.
    x = (rightRotate8_64(x) + y) ^ s;
    y = leftRotate3_64(y) ^ x;
    pack64(output, x);
    pack64(output + 8, y);
#endif
}

void SpeckTiny::decryptBlock(uint8_t *output, const uint8_t *input)
{
    // Decryption is not supported by SpeckTiny.  Use SpeckSmall instead.
}

void SpeckTiny::clear()
{
    clean(k);
}
