/*
 * Copyright © 2013 Siarhei Siamashka <siarhei.siamashka@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Adapted for RPi: Copyright 2013 Harm Hanemaaijer (fgenfb@yahoo.com)
 */

/* Prevent the stack from becoming executable */
#if defined(__linux__) && defined(__ELF__)
.section .note.GNU-stack,"",%progbits
#endif

#ifdef __arm__

.text
.syntax unified
.fpu vfp
.arch armv6j
.object_arch armv4
.arm
.altmacro
.p2align 2

/******************************************************************************/

.macro asm_function function_name
    .global \function_name
.func \function_name
\function_name:
.endm

/******************************************************************************/

/*
 * aligned_fetch_fbmem_to_scratch_arm(int numbytes, void *scratch, void *fbmem)
 *
 * Both 'scratch' and 'fbmem' pointers must be 32 bytes aligned.
 * The value in 'numbytes' is also rounded up to a multiple of 32 bytes.

 * The only purpose of this code is to attempt minimizing penalty incured
 * by doing uncached reads from memory (for example framebuffer). We are
 * trying to do the largest possible perfectly aligned reads to fetch
 * data into a temporary scratch buffer in L1 cache.
 *
 * Assumptions: scratch and fbmem 32-byte aligned, numbytes >= 1.
 */

asm_function aligned_fetch_fbmem_to_scratch_arm
    SIZE        .req r0
    DST         .req r1
    SRC         .req r2

    push        {r4, r11}
    /* Round size up to multiple of 32. */
    eor		r3, r4, r4         // r3 = 0
    add         r4, r3, SIZE       // r4 = SIZE
    add         r3, r3, #32        // r3 = 32
    and         r4, r4, #31        // r4 = SIZE & 31
    sub         r3, r3, r4         // r3 = 32 - (SIZE & 31)
    add         SIZE, SIZE, r3     // SIZE += 32 - (SIZE & 31)
0:
    /*
     * Aligned load from the source (framebuffer).
     * The following lines of code were unceremoniously extracted
     * from Simon H and tufty's memcpy inner loop used in
     * Raspbian memcpy.
      */
    ldmia       SRC!, {r4, r7}
    ldmia       SRC!, {r8, r11}
    stmia       DST!, {r4, r7}
    pld         [SRC, #128]
    subs        SIZE, SIZE, #32
    stmia       DST!, {r8, r11}
    bne          0b
1:
    pop         {r4, r11}
    bx          lr

    .unreq      SIZE
    .unreq      DST
    .unreq      SRC
.endfunc

#endif
