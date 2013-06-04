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
 * Modified by Harm Hanemaaijer <fgenfb@yahoo.com>:
 * 1. Add ".type <function_name>, function" to function definition macro, which
 *    was required for correct linkage on my platform.
 * 2. Implement aligned_fetch_fbmem_to_scratch_arm based on armv5te_memcpy.
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
.type \function_name, function
\function_name:
.endm

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
    add     r0, r0, #31
    pld     [r2, #0]
    bic     r0, r0, #31
    stmfd   sp!, {r4-r7}
    cmp     r0, #96
    stmfd   sp!, {r8-r11}
    mov     r9, #32           /* Prefetch offset 32. */
    /* Only do prefetch for chunks we are actually going to read. */
    blt     2f
    pld     [r2, r9]
1:
    ldr     r11, [r1]         /* Fetch destination into L1 cache. */
    sub     r0, r0, #32
    ldmia   r2!, {r3-r6, r7, r8, r10, r11}
    cmp     r0, #96
    pld     [r2, r9]
    stmia   r1!, {r3-r6}
    stmia   r1!, {r7, r8, r10, r11}
    bge     1b
2:
    ldr     r11, [r1]
    sub     r0, r0, #32
    ldmia   r2!, {r3-r6, r7, r8, r10, r11}
    cmp     r0, #32
    stmia   r1!, {r3-r6}
    stmia   r1!, {r7, r8, r10, r11}
    bge     2b

    ldmfd   sp!, {r8-r11}
    ldmfd   sp!, {r4-r7}

    bx      lr

.endfunc

#endif
