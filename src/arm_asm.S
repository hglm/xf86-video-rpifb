/*
 * Copyright © 2006-2008, 2013 Siarhei Siamashka <siarhei.siamashka@gmail.com>
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
 * Copyright 2013 Harm Hanemaaijer <fgenfb@yahoo.com>
 *
 * 1. Add ".type <function_name>, function" to function definition macro, which
 *    was required for correct linkage on my platform.
 * 2. Add non-overfetching memcpy version with a plethora of optimizations and variants using
 *    macros.
 *    To do: -- Fully implement line_size == 64 and write_align == 64 for unaligned case.
 *
 * On the RPi platform, a good choice is armv5te_no_overfetch_align_16_block_write_16_preload_early_128,
 * closely followed by armv5te_no_overfetch_align_16_block_write_16_preload_early_96. For
 * CPU-cache based work loads armv5te_no_overfetch_align_16_block_write_16_preload_96 might be
 * a little faster.
 *
 * On the Allwinner A10 platform, armv5te_no_overfetch_align_32_block_write_32_preload_early_192
 * is a good choice.
 *
 * On the Allwinner plaform, the optimized memcpy is faster; on the RPi libcofi does relatively well
 * and is a little faster in some tests for aligned regions while being slower for unaligned
 * copies.
 */

/* Prevent the stack from becoming executable */
#if defined(__linux__) && defined(__ELF__)
.section .note.GNU-stack,"",%progbits
#endif

#ifdef __arm__

.text
.syntax unified
.fpu neon
.arch armv7a
.object_arch armv4
.arm
.altmacro
.p2align 2

/******************************************************************************/

.macro asm_function function_name
    .global \function_name
.func \function_name
.type \function_name, function
.p2align 5
\function_name:
.endm

/******************************************************************************/

/*
 * Helper macro for memcpy function, it can copy data from source (r1) to 
 * destination (r0) buffers fixing alignment in the process. Destination
 * buffer should be aligned already (4 bytes alignment is required.
 * Size of the block to copy is in r2 register
 */
.macro  UNALIGNED_MEMCPY shift
    sub     r1, #(\shift)
    ldr     ip, [r1], #4

    tst     r0, #4
    movne   r3, ip, lsr #(\shift * 8)
    ldrne   ip, [r1], #4
    subne   r2, r2, #4
    orrne   r3, r3, ip, asl #(32 - \shift * 8)
    strne   r3, [r0], #4

    tst     r0, #8
    movne   r3, ip, lsr #(\shift * 8)
    ldmiane r1!, {r4, ip}
    subne   r2, r2, #8
    orrne   r3, r3, r4, asl #(32 - \shift * 8)
    movne   r4, r4, lsr #(\shift * 8)
    orrne   r4, r4, ip, asl #(32 - \shift * 8)
    stmiane r0!, {r3-r4}
    cmp     r2, #32
    blt     3f
    pld     [r1, #48]
    stmfd   sp!, {r7, r8, r9, r10, r11}
    add     r3, r1, #128
    bic     r3, r3, #31
    sub     r9, r3, r1
1:
    pld     [r1, r9]
    subs    r2, r2, #32
    movge   r3, ip, lsr #(\shift * 8)
    ldmiage r1!, {r4-r6, r7, r8, r10, r11, ip}
    orrge   r3, r3, r4, asl #(32 - \shift * 8)
    movge   r4, r4, lsr #(\shift * 8)
    orrge   r4, r4, r5, asl #(32 - \shift * 8)
    movge   r5, r5, lsr #(\shift * 8)
    orrge   r5, r5, r6, asl #(32 - \shift * 8)
    movge   r6, r6, lsr #(\shift * 8)
    orrge   r6, r6, r7, asl #(32 - \shift * 8)
    stmiage r0!, {r3-r6}
    movge   r7, r7, lsr #(\shift * 8)
    orrge   r7, r7, r8, asl #(32 - \shift * 8)
    movge   r8, r8, lsr #(\shift * 8)
    orrge   r8, r8, r10, asl #(32 - \shift * 8)
    movge   r10, r10, lsr #(\shift * 8)
    orrge   r10, r10, r11, asl #(32 - \shift * 8)
    movge   r11, r11, lsr #(\shift * 8)
    orrge   r11, r11, ip, asl #(32 - \shift * 8)
    stmiage r0!, {r7, r8, r10, r11}
    bgt     1b
2:
    ldmfd   sp!, {r7, r8, r9, r10, r11}
3:  /* copy remaining data */
    tst     r2, #16
    movne   r3, ip, lsr #(\shift * 8)
    ldmiane r1!, {r4-r6, ip}
    orrne   r3, r3, r4, asl #(32 - \shift * 8)
    movne   r4, r4, lsr #(\shift * 8)
    orrne   r4, r4, r5, asl #(32 - \shift * 8)
    movge   r5, r5, lsr #(\shift * 8)
    orrge   r5, r5, r6, asl #(32 - \shift * 8)
    movge   r6, r6, lsr #(\shift * 8)
    orrge   r6, r6, ip, asl #(32 - \shift * 8)
    stmiane r0!, {r3-r6}

    tst     r2, #8
    movne   r3, ip, lsr #(\shift * 8)
    ldmiane r1!, {r4, ip}
    orrne   r3, r3, r4, asl #(32 - \shift * 8)
    movne   r4, r4, lsr #(\shift * 8)
    orrne   r4, r4, ip, asl #(32 - \shift * 8)
    stmiane r0!, {r3-r4}

    tst     r2, #4
    movne   r3, ip, lsr #(\shift * 8)
    ldrne   ip, [r1], #4
    sub     r1, r1, #(4 - \shift)
    orrne   r3, r3, ip, asl #(32 - \shift * 8)
    strne   r3, [r0], #4

    tst     r2, #2
    ldrbne  r3, [r1], #1
    ldrbne  r4, [r1], #1
    ldr     r5, [sp], #4
    strbne  r3, [r0], #1
    strbne  r4, [r0], #1

    tst     r2, #1
    ldrbne  r3, [r1], #1
    ldr     r6, [sp], #4
    strbne  r3, [r0], #1

    pop     {r0, r4}

    bx      lr
.endm

/*
 * Memcpy function with Raspberry Pi specific aligned prefetch, based on
 * https://garage.maemo.org/plugins/scmsvn/viewcvs.php/mplayer/trunk/fastmem-arm9/fastmem-arm9.S
 */
asm_function memcpy_armv5te
    cmp     r2, #20
    blt     9f
    /* copy data until destination address is 4 bytes aligned */
    tst     r0, #1
    ldrbne  r3, [r1], #1
    stmfd   sp!, {r0, r4}
    subne   r2, r2, #1
    strbne  r3, [r0], #1
    tst     r0, #2
    ldrbne  r3, [r1], #1
    ldrbne  r4, [r1], #1
    stmfd   sp!, {r5, r6}
    subne   r2, r2, #2
    orrne   r3, r3, r4, asl #8
    strhne  r3, [r0], #2
    /* destination address is 4 bytes aligned */
    /* now we should handle 4 cases of source address alignment */
    tst     r1, #1
    bne     6f
    tst     r1, #2
    bne     7f

    /* both source and destination are 4 bytes aligned */
    stmfd   sp!, {r7, r8, r9, r10, r11}
    tst     r0, #4
    ldrne   r4, [r1], #4
    subne   r2, r2, #4
    strne   r4, [r0], #4
    tst     r0, #8
    ldmiane r1!, {r3-r4}
    add     r9, r1, #96
    subne   r2, r2, #8
    bic     r9, r9, #31
    stmiane r0!, {r3-r4}
    sub     r9, r9, r1
1:
    subs    r2, r2, #32
    ldmiage r1!, {r3-r6, r7, r8, r10, r11}
    pld     [r1, r9]
    stmiage r0!, {r3-r6}
    stmiage r0!, {r7, r8, r10, r11}
    bgt     1b
2:
    ldmfd   sp!, {r7, r8, r9, r10, r11}
    tst     r2, #16
    ldmiane r1!, {r3-r6}
    stmiane r0!, {r3-r6}
    tst     r2, #8
    ldmiane r1!, {r3-r4}
    stmiane r0!, {r3-r4}
    tst     r2, #4
    ldrne   r3, [r1], #4
    mov     ip, r0
    strne   r3, [ip], #4
    tst     r2, #2
    ldrhne  r3, [r1], #2
    ldmfd   sp!, {r5, r6}
    strhne  r3, [ip], #2
    tst     r2, #1
    ldrbne  r3, [r1], #1
    ldmfd   sp!, {r0, r4}
    strbne  r3, [ip], #1

    bx      lr

6:
    tst    r1, #2
    bne    8f
    UNALIGNED_MEMCPY 1
7:
    UNALIGNED_MEMCPY 2
8:
    UNALIGNED_MEMCPY 3
9:
    stmfd  sp!, {r0, r4}
1:  subs   r2, r2, #3
    ldrbge ip, [r0]
    ldrbge r3, [r1], #1
    ldrbge r4, [r1], #1
    ldrbge ip, [r1], #1
    strbge r3, [r0], #1
    strbge r4, [r0], #1
    strbge ip, [r0], #1
    bge    1b
    adds   r2, r2, #2
    ldrbge r3, [r1], #1
    mov    ip, r0
    ldr    r0, [sp], #4
    strbge r3, [ip], #1
    ldrbgt r3, [r1], #1
    ldr    r4, [sp], #4
    strbgt r3, [ip], #1
    bx     lr
.endfunc

/*
 * Helper macro for non-overfetching version.
 *
 * If preload_early == 1,
 * r6 is the address of the 32-byte aligned region containing the last source byte.
 * r4 is the address of the 32-byte aligned region where the last preload occured
 */

.macro  UNALIGNED_MEMCPY_VARIANT shift, line_size, write_align, block_write_size, preload_offset, preload_early, overfetch
    sub     r1, #(\shift)
.if \preload_early == 1
    add     r5, r4, #\line_size
.endif
    ldr     ip, [r1], #4
.if \preload_early == 1
.if \overfetch == 0
    cmp     r6, r5
    /* Only preload if the source region extends into it. */
    blt     5f
.endif
    pld     [r5]
5:
.if \write_align >= 32
    /* Need a place to store r5. */
    push    {r5}
.endif
.endif

    tst     r0, #4
    movne   r3, ip, lsr #(\shift * 8)
    ldrne   ip, [r1], #4
    subne   r2, r2, #4
    orrne   r3, r3, ip, asl #(32 - \shift * 8)
    strne   r3, [r0], #4

    tst     r0, #8
    movne   r3, ip, lsr #(\shift * 8)
    ldmiane r1!, {r4, ip}
    subne   r2, r2, #8
    orrne   r3, r3, r4, asl #(32 - \shift * 8)
    movne   r4, r4, lsr #(\shift * 8)
    orrne   r4, r4, ip, asl #(32 - \shift * 8)
    stmiane r0!, {r3-r4}

.if \write_align >= 32
    tst     r0, #16
    movne   r3, ip, lsr #(\shift * 8)
    ldmiane r1!, {r4-r6, ip}
    subne   r2, r2, #16
    orrne   r3, r3, r4, asl #(32 - \shift * 8)
    movne   r4, r4, lsr #(\shift * 8)
    orrne   r4, r4, r5, asl #(32 - \shift * 8)
    movne   r5, r5, lsr #(\shift * 8)
    orrne   r5, r5, r6, asl #(32 - \shift * 8)
    movne   r6, r6, lsr #(\shift * 8)
    orrne   r6, r6, ip, asl #(32 - \shift * 8)
    stmiane r0!, {r3-r6}
.endif

    cmp     r2, #32
.if \preload_early == 1 && \write_align >= 32
    pop     {r5}
.endif
    blt     3f
.if \preload_offset != 0
.if \overfetch == 1
    cmp     r2, #64
.else
    cmp     r2, #\preload_offset
.endif
.endif
    stmfd   sp!, {r7, r8, r9, r10, r11}
.if \preload_offset != 0
    add     r10, r1, #\preload_offset
.if \preload_early == 1 && \preload_offset >= 64
    add     r5, r5, #(\line_size * 2)
.endif
    bic     r10, r10, #31
    sub     r9, r10, r1
.if \overfetch == 0
    /* If there are <= preload_offset bytes to go, skip the main loop. */
    ble     4f
.else
    blt     1f
.endif
.if \preload_early == 1 && \preload_offset >= 64 && block_write_size >= 16
    mov     r11, r5
    pld     [r5, #-\line_size]
    /* The last preload already done is at [r11 - line_size]. */
    /* The next preload in the main loop will happen at [r10]. */
    /* If r11 < r10, we want to do an extra preload at [r11]. */
#if 1
18:
    cmp     r11, r10
    add     r11, #64
    movlt   r3, ip, lsr #(\shift * 8)
    ldmialt r1!, {r4-r6, r7}
    orrlt   r3, r3, r4, asl #(32 - \shift * 8)
    movlt   r4, r4, lsr #(\shift * 8)
    bge     1f
    cmp     r2, #(\preload_offset + 32)
    pld     [r11, #-64]
    sub     r2, r2, #32
    orr     r4, r4, r5, asl #(32 - \shift * 8)
    mov     r5, r5, lsr #(\shift * 8)
    orr     r5, r5, r6, asl #(32 - \shift * 8)
    mov     r6, r6, lsr #(\shift * 8)
    orr     r6, r6, r7, asl #(32 - \shift * 8)
    mov     r7, r7, lsr #(\shift * 8)
    stmia   r0!, {r3-r6}
    mov     r3, r7
    ldmia   r1!, {r4, r5, r6, ip}
.if \line_size == 32
    pld     [r11, #-32]
.endif
    orr     r3, r3, r4, asl #(32 - \shift * 8)
    add     r10, r1, r9
    mov     r4, r4, lsr #(\shift * 8)
    orr     r4, r4, r5, asl #(32 - \shift * 8)
    mov     r5, r5, lsr #(\shift * 8)
    orr     r5, r5, r6, asl #(32 - \shift * 8)
    mov     r6, r6, lsr #(\shift * 8)
    orr     r6, r6, ip, asl #(32 - \shift * 8)
    stmia   r0!, {r3, r4, r5, r6}
    bgt     18b
.if \overfetch == 0
    b       4f
.endif
#endif
.endif
1:
.if \overfetch == 1
    cmp     r2, #(32 + 32)
.else
    cmp     r2, #(\preload_offset + 32)
.endif
    pld     [r1, r9]
    mov     r3, ip, lsr #(\shift * 8)
    ldmia   r1!, {r4-r6, r7, r8, r10, r11, ip}
    orr     r3, r3, r4, asl #(32 - \shift * 8)
    mov     r4, r4, lsr #(\shift * 8)
    sub     r2, r2, #32
    orr     r4, r4, r5, asl #(32 - \shift * 8)
    mov     r5, r5, lsr #(\shift * 8)
.if \block_write_size == 8
    stmia   r0!, {r3-r4}
.endif
    orr     r5, r5, r6, asl #(32 - \shift * 8)
    mov     r6, r6, lsr #(\shift * 8)
    orr     r6, r6, r7, asl #(32 - \shift * 8)
    mov     r7, r7, lsr #(\shift * 8)
.if \block_write_size == 16
    stmia   r0!, {r3-r6}
.endif
.if \block_write_size == 8
    stmia   r0!, {r5-r6}
.endif
    orr     r7, r7, r8, asl #(32 - \shift * 8)
    mov     r8, r8, lsr #(\shift * 8)
    orr     r8, r8, r10, asl #(32 - \shift * 8)
    mov     r10, r10, lsr #(\shift * 8)
.if \block_write_size == 8
    stmia   r0!, {r7-r8}
.endif
    orr     r10, r10, r11, asl #(32 - \shift * 8)
    mov     r11, r11, lsr #(\shift * 8)
    orr     r11, r11, ip, asl #(32 - \shift * 8)
.if \block_write_size == 32
    stmia   r0!, {r3-r6, r7, r8, r10, r11}
.endif
.if \block_write_size == 16
    stmia   r0!, {r7, r8, r10, r11}
.endif
.if \block_write_size == 8
    stmia   r0!, {r10-r11}
.endif
    bge     1b
.endif /* preload_offset != 0 */
.if \overfetch == 0
4:
    cmp     r2, #(32 + 32)
    mov     r3, ip, lsr #(\shift * 8)
    ldmia   r1!, {r4-r6, r7, r8, r10, r11, ip}
    orr     r3, r3, r4, asl #(32 - \shift * 8)
    sub     r2, r2, #32
    mov     r4, r4, lsr #(\shift * 8)
    orr     r4, r4, r5, asl #(32 - \shift * 8)
    mov     r5, r5, lsr #(\shift * 8)
.if \block_write_size == 8
    stmia   r0!, {r3-r4}
.endif
    orr     r5, r5, r6, asl #(32 - \shift * 8)
    mov     r6, r6, lsr #(\shift * 8)
    orr     r6, r6, r7, asl #(32 - \shift * 8)
    mov     r7, r7, lsr #(\shift * 8)
.if \block_write_size == 16
    stmia   r0!, {r3-r6}
.endif
.if \block_write_size == 8
    stmia   r0!, {r5-r6}
.endif
    orr     r7, r7, r8, asl #(32 - \shift * 8)
    mov     r8, r8, lsr #(\shift * 8)
    orr     r8, r8, r10, asl #(32 - \shift * 8)
    mov     r10, r10, lsr #(\shift * 8)
.if \block_write_size == 8
    stmia   r0!, {r7-r8}
.endif
    orr     r10, r10, r11, asl #(32 - \shift * 8)
    mov     r11, r11, lsr #(\shift * 8)
    orr     r11, r11, ip, asl #(32 - \shift * 8)
.if \block_write_size == 32
    stmia   r0!, {r3-r6, r7, r8, r10, r11}
.endif
.if \block_write_size == 16
    stmia   r0!, {r7, r8, r10, r11}
.endif
.if \block_write_size == 8
    stmia   r0!, {r10-r11}
.endif
    bge     4b
.endif /* overfetch == 0 */
21:
    ldmfd   sp!, {r7, r8, r9, r10, r11}
3:  /* copy remaining data */
    tst     r2, #16
    movne   r3, ip, lsr #(\shift * 8)
    ldmiane r1!, {r4-r6, ip}
    orrne   r3, r3, r4, asl #(32 - \shift * 8)
    movne   r4, r4, lsr #(\shift * 8)
    orrne   r4, r4, r5, asl #(32 - \shift * 8)
    movge   r5, r5, lsr #(\shift * 8)
    orrge   r5, r5, r6, asl #(32 - \shift * 8)
    movge   r6, r6, lsr #(\shift * 8)
    orrge   r6, r6, ip, asl #(32 - \shift * 8)
    stmiane r0!, {r3-r6}

    tst     r2, #8
    movne   r3, ip, lsr #(\shift * 8)
    ldmiane r1!, {r4, ip}
    orrne   r3, r3, r4, asl #(32 - \shift * 8)
    movne   r4, r4, lsr #(\shift * 8)
    orrne   r4, r4, ip, asl #(32 - \shift * 8)
    stmiane r0!, {r3-r4}

    tst     r2, #4
    movne   r3, ip, lsr #(\shift * 8)
    ldrne   ip, [r1], #4
    sub     r1, r1, #(4 - \shift)
    orrne   r3, r3, ip, asl #(32 - \shift * 8)
    strne   r3, [r0], #4

    tst     r2, #2
    ldrbne  r3, [r1], #1
    ldrbne  r4, [r1], #1
    ldr     r5, [sp], #4
    strbne  r3, [r0], #1
    strbne  r4, [r0], #1

    tst     r2, #1
    ldrbne  r3, [r1], #1
    ldr     r6, [sp], #4
    strbne  r3, [r0], #1

    pop     {r0, r4}

    bx      lr
.endm

/*
 * Macro that defines the main body of a memcpy version with optional no over-fetching
 * beyond the source memory region.
 *
 * line_size must be 32 or 64.
 * write_align must be 32 or 16, or 64.
 * block_write_size must be 32, 16 or 8.
 * preload_offset must be a multiple of 32, 96 was the default setting. When preload_offset is 0,
 * no preload instructions will be generated at all.
 * preload_early must be 0 or 1.
 * overfetch must be 0 or 1.
 *
 * If line_size is 64, write_align must be 64, block_write_size must be 32, and preload_offset
 * must be a multiple of 64.
 */

.macro MEMCPY_VARIANT line_size, write_align, block_write_size, preload_offset, preload_early, overfetch
.if \preload_early == 1
    bic     r3, r1, #(\line_size - 1)
.endif
.if \write_align >= 32
    cmp     r2, #36
.else
    cmp     r2, #20
.endif
.if \preload_early == 1
    pld     [r3]
.endif
    stmfdge sp!, {r0, r4}
.if \preload_early == 1
    movge   r4, r3
.endif
    blt     9f
    /* copy data until destination address is 4 bytes aligned */
    tst     r0, #1
    ldrbne  r3, [r1], #1
    subne   r2, r2, #1
    strbne  r3, [r0], #1
.if \preload_early == 1
    /* The standard code path for early preload will preload up to [r3 + line_size * 4]. */
    /* ([r3 + line_size * 2] if line_size = 64). */
    /* To avoid preloads beyond the source region for medium sizes, there is a seperate */
    /* path for them when early preload is enabled. */
.if \line_size == 64
    cmp     r2, #(\line_size * 3)
.else
    cmp     r2, #(\line_size * 5)
.endif
    pld     [r4, #\line_size]
.endif
    stmfd   sp!, {r5, r6}
.if \preload_early == 1
    blt     17f
.endif
    tst     r0, #2
    ldrbne  r3, [r1], #1
    ldrbne  r5, [r1], #1
    subne   r2, r2, #2
    orrne   r3, r3, r5, asl #8
.if \preload_early == 1
    /* Determine the address of the last byte. */
    add     r6, r1, r2
.endif
    strhne  r3, [r0], #2
    /* destination address is 4 bytes aligned */
    /* now we should handle 4 cases of source address alignment */
    tst     r1, #1
.if \preload_early == 1
.if \overfetch == 0
    sub     r6, r6, #1
.endif
.if \line_size == 32
    pld     [r4, #(\line_size * 2)]
    add     r4, #(\line_size * 2)
.else
    /* When the line size is 64 bytes, skip this preload step. */
    add     r4, #\line_size
.endif
.if \overfetch == 0
    bic     r6, r6, #(\line_size - 1)
.endif
.endif
    bne     6f
    tst     r1, #2
    bne     7f

    /* both source and destination are 4 bytes aligned */
    stmfd   sp!, {r7, r8, r9, r10, r11}
    tst     r0, #4
.if \preload_early == 1
    pld     [r4, #(\line_size)]
.endif
    ldrne   r5, [r1], #4
    subne   r2, r2, #4
    strne   r5, [r0], #4
    tst     r0, #8
    ldmiane r1!, {r3, r5}
    subne   r2, r2, #8
    stmiane r0!, {r3, r5}
.if \write_align == 32 || \write_align == 64
    tst     r0, #16
    ldmiane r1!, {r3, r5-r7}
    subne   r2, r2, #16
    stmiane r0!, {r3, r5-r7}
.endif
.if \write_align == 64
    cmp     r2, #32
    blt     14f
    tst     r0, #32
    ldmiane r1!, {r3, r5-r11}
    subne   r2, r2, #32
    stmiane r0!, {r3, r5-r11}
.endif
    /* Source is now write_align bytes aligned. */
20:
.if \preload_offset == 0
    cmp     r2, #32
    blt     2f
.elseif \write_align == 64 || \line_size == 64
    cmp     r2, #64
.if \line_size == 64 && \write_align == 32
    add     r9, r1, #\preload_offset
.endif
.if \preload_early == 1
.if \line_size == 32
    pld     [r4, #(\line_size * 2)]
    add     r4, #(\line_size * 3)
.else
    add     r4, #(\line_size * 2)
.endif
.endif
.if \line_size == 64 && \write_align == 32
    bic     r9, r9, #63
.else
    mov     r9, #\preload_offset
.if \preload_early == 1
    add     r11, r1, #\preload_offset
.endif
.endif
    blt     2f
    cmp     r2, #(\preload_offset + 64)
.if \line_size == 64 && \write_align == 32
.if \preload_early == 1
    mov     r11, r9
.endif
    sub     r9, r9, r1
.endif
.if \overfetch == 1
    ble     1f
.else
    ble     10f
.endif
.elseif \write_align == 32
    /* In the case of line_size == 32 and write_align == 32 r9 will be equal to preload_offset. */
    cmp     r2, #32
.if \preload_early == 1
    pld     [r4, #(\line_size * 2)]
    add     r4, #(\line_size * 3)
.endif
    mov     r9, #\preload_offset
.if \preload_early == 1
    add     r11, r1, #\preload_offset
.endif
    blt     2f
    cmp     r2, #(\preload_offset + 32)
.if \overfetch == 1
    ble     1f
.else
    ble     10f
.endif
.else // write_align == 16
    cmp     r2, #32
    add     r9, r1, #\preload_offset
.if \preload_early == 1
    pld     [r4, #(\line_size * 2)]
    add     r4, #(\line_size * 3)
.endif
    bic     r9, r9, #31
    /* If there are less than 32 bytes to go, skip all loops. */
    blt     2f
    cmp     r2, #(\preload_offset + 32)
.if \preload_early == 1
    mov     r11, r9
.endif
    sub     r9, r9, r1
    /* If there are <= (preload_offset + 32) bytes to go, skip the main loop. */
.if \overfetch == 1
    ble     1f
.else
    ble     10f
.endif
.endif
19:
.if \preload_offset != 0
.if \preload_early == 1
.if \block_write_size >= 16 && \preload_offset >= 96
    /* The last preload already done is at [ip]. */
    /* The next preload in the main loop will happen at [r11 + line_size]. */
    /* If ip < r11, we want to do an extra preload at [ip + line_size]. */
    /* Note: if line_size is 64 and write alignment is 64, write alignment */
    /* may become unaligned. */
    cmp     r4, r11
    mov     ip, r4
    pld     [r4]
    add     ip, #64
    ldmialt r1!, {r3-r6, r7, r8, r10, r11}
    bge     1f
.if \line_size == 64 || \write_align == 64
    cmp     r2, #(\preload_offset + 64 + 32)
.if \line_size == 64
    pld     [ip]
.else
    pld     [ip, #-32]
.endif
.else
    cmp     r2, #(\preload_offset + 32 + 32)
    pld     [ip, #-32]
.endif
    sub     r2, r2, #32
.if \block_write_size == 32
    stmia   r0!, {r3-r6, r7, r8, r10, r11}
    add     r4, r1, r9
.else
    stmia   r0!, {r3-r6}
    add     r4, r1, r9
    stmia   r0!, {r7, r8, r10, r11}
.endif
.if \line_size == 32
    pld     [ip]
.endif
.if \overfetch  == 1
    ble     1f
.else
    ble     10f
.endif
#if 1
    /* Do other extra preloads if required. */
13:
    cmp     ip, r4
    add     ip, #64
    ldmialt r1!, {r3-r6, r7, r8, r10, r11}
    bge     1f
.if \line_size == 64 || \write_align == 64
    cmp     r2, #(\preload_offset + 64 + 32)
.if \line_size == 64
    pld     [ip]
.else
    pld     [ip, #-32]
.endif
.else
    cmp     r2, #(\preload_offset + 32 + 32)
    pld     [ip, #-32]
.endif
    sub     r2, r2, #32
.if \block_write_size == 32
    stmia   r0!, {r3-r6, r7, r8, r10, r11}
    add     r4, r1, r9
.else
    stmia   r0!, {r3-r6}
    add     r4, r1, r9
    stmia   r0!, {r7, r8, r10, r11}
.endif
.if \line_size == 32
    pld     [ip]
.endif
    bgt     13b
.if \overfetch == 0
    b       10f
.endif
#endif
.else // !(block_write_size >= 16 && preload_offset >= 96)
    pld     [r4]
.endif
.endif
1:
.if \line_size == 64 || \write_align == 64
.if \overfetch == 1
    cmp     r2, #(64 + 64)
.else
    cmp     r2, #(\preload_offset + 64 + 64)
.endif
    ldmia   r1!, {r3-r6, r7, r8, r10, r11}
.if \line_size == 32
    pld     [r1, r9]
.endif
    sub     r2, r2, #64
    stmia   r0!, {r3-r6, r7, r8, r10, r11}
    ldmia   r1!, {r3-r6, r7, r8, r10, r11}
    pld     [r1, r9]
    stmia   r0!, {r3-r6, r7, r8, r10, r11}
.else
.if \overfetch == 1
    cmp     r2, #(32 + 32)
.else
    cmp     r2, #(\preload_offset + 32 + 32)
.endif
.if \block_write_size == 32
    ldmia   r1!, {r3-r6, r7, r8, r10, r11}
    sub     r2, r2, #32
    pld     [r1, r9]
    stmia   r0!, {r3-r6, r7, r8, r10, r11}
.endif
.if \block_write_size == 16
    ldmia   r1!, {r3-r6, r7, r8, r10, r11}
    sub     r2, r2, #32
    pld     [r1, r9]
    stmia   r0!, {r3-r6}
    stmia   r0!, {r7, r8, r10, r11}
.endif
.if \block_write_size == 8
    ldmia   r1!, {r3-r6, r7, r8, r10, r11}
    sub     r2, r2, #32
    stmia   r0!, {r3-r4}
    stmia   r0!, {r5-r6}
    pld     [r1, r9]
    stmia   r0!, {r7-r8}
    stmia   r0!, {r10-r11}
.endif
.endif /* line_size ==  64 */
    bge     1b
.endif /* preload_offset != 0 */
.if \overfetch == 0
10:
.if \line_size == 64 || \write_align == 64
    cmp     r2, #(64 + 64)
    ldmia   r1!, {r3-r6, r7, r8, r10, r11}
    sub     r2, r2, #64
    stmia   r0!, {r3-r6, r7, r8, r10, r11}
    ldmia   r1!, {r3-r6, r7, r8, r10, r11}
    stmia   r0!, {r3-r6, r7, r8, r10, r11}
.else
.if \block_write_size == 32
    cmp     r2, #(32 + 32)
    ldmia   r1!, {r3-r6, r7, r8, r10, r11}
    sub     r2, r2, #32
    stmia   r0!, {r3-r6, r7, r8, r10, r11}
.endif
.if \block_write_size == 16
    cmp     r2, #(32 + 32)
    ldmia   r1!, {r3-r6, r7, r8, r10, r11}
    sub     r2, r2, #32
    stmia   r0!, {r3-r6}
    stmia   r0!, {r7, r8, r10, r11}
.endif
.if \block_write_size == 8
    cmp     r2, #(32 + 32)
    ldmia   r1!, {r3-r6, r7, r8, r10, r11}
    sub     r2, r2, #32
    stmia   r0!, {r3-r4}
    stmia   r0!, {r5-r6}
    stmia   r0!, {r7-r8}
    stmia   r0!, {r10-r11}
.endif
.endif /* line_size == 64 || write_align == 64  */
    bge     10b
.endif /* overfetch == 0 */
2:
.if \line_size == 64 || \write_align == 64
    tst     r2, #32
    ldmiane r1!, {r3-r6, r7, r8, r10, r11}
    stmiane r0!, {r3-r6, r7, r8, r10, r11}
.endif
14:
    ldmfd   sp!, {r7, r8, r9, r10, r11}
    tst     r2, #16
    ldmiane r1!, {r3-r6}
    stmiane r0!, {r3-r6}
    tst     r2, #8
    ldmiane r1!, {r3-r4}
    stmiane r0!, {r3-r4}
    tst     r2, #4
    ldrne   r3, [r1], #4
    mov     ip, r0
    strne   r3, [ip], #4
    tst     r2, #2
    ldrhne  r3, [r1], #2
    ldmfd   sp!, {r5, r6}
    strhne  r3, [ip], #2
    tst     r2, #1
    ldrbne  r3, [r1], #1
    ldmfd   sp!, {r0, r4}
    strbne  r3, [ip], #1

    bx      lr

12:
    cmp     r2, #32
    bge     10b
    b       2b
6:
    tst    r1, #2
    bne    8f
    UNALIGNED_MEMCPY_VARIANT 1, \line_size, \write_align, \block_write_size, \preload_offset, \preload_early, \overfetch
7:
    UNALIGNED_MEMCPY_VARIANT 2, \line_size, \write_align, \block_write_size, \preload_offset, \preload_early, \overfetch
8:
    UNALIGNED_MEMCPY_VARIANT 3, \line_size, \write_align, \block_write_size, \preload_offset, \preload_early, \overfetch
    .p2align 4
9:
    /*
     * Size < 36.
     * Test whether both source and destination
     * are word-aligned, otherwise use a byte-copier.
     */
    tst    r0, #3
    andseq r3, r1, #3
    stmfd  sp!, {r0, r4}
    beq    22f
    tst    r0, #1
    andseq r3, r1, #1
    beq    3f
    /* Byte copier. */
1:
    subs   r2, r2, #3
    ldrbge ip, [r0]
    ldrbge r3, [r1], #1
    ldrbge r4, [r1], #1
    ldrbge ip, [r1], #1
    strbge r3, [r0], #1
    strbge r4, [r0], #1
    strbge ip, [r0], #1
    bge    1b
    adds   r2, r2, #2
    ldrbge r3, [r1], #1
    mov    ip, r0
    ldr    r0, [sp], #4
    strbge r3, [ip], #1
    ldrbgt r3, [r1], #1
    ldr    r4, [sp], #4
    strbgt r3, [ip], #1
    bx     lr
22:
    /* Copy words. */
    subs   r2, r2, #4
    ldrge  r3, [r1], #4
    strge  r3, [r0], #4
    bge    22b
    tst    r2, #2
    mov    ip, r0
    ldrhne r3, [r1], #2
    strhne r3, [ip], #2
    tst    r2, #1
    ldrbne r3, [r1], #1
    ldmfd  sp!, {r0, r4}
    strbne r3, [ip], #1
    bx     lr
3:
    /* Copy half-words. */
    subs   r2, r2, #2
    ldrhge r3, [r1], #2
    strhge r3, [r0], #2
    bge    3b
    tst    r2, #1
    mov    ip, r0
    ldrbne r3, [r1], #1
    ldmfd  sp!, {r0, r4}
    strbne r3, [ip], #1
    bx     lr

    /* Special code path for medium sizes when early preload is enabled. */
    .p2align 4
17:
    tst     r0, #2
    ldrbne  r3, [r1], #1
    ldrbne  r5, [r1], #1
    subne   r2, r2, #2
    orrne   r3, r3, r5, asl #8
.if \overfetch == 0
    /* Determine the address of the last byte. */
    add     r6, r1, r2
.endif
    strhne  r3, [r0], #2

    /* destination address is 4 bytes aligned */
    /* now we should handle 4 cases of source address alignment */
    tst     r1, #1
.if \overfetch == 0
    sub     r6, r6, #1
.endif
    add     r4, r4, \line_size
.if \overfetch == 0
    bic     r6, r6, #(\line_size - 1)
.endif
    bne     6b
    tst     r1, #2
    add     r5, r4, #\line_size
    bne     7b

    /* both source and destination are 4 bytes aligned */
.if \overfetch == 0
    cmp     r6, r5
.endif
    stmfd   sp!, {r7, r8, r9, r10, r11}
.if \overfetch == 0
    mov     r8, r6
    /* Only preload if the source region extends into it. */
    movlt   r4, r5
    blt     15f
.endif
    pld     [r5]
    add     r4, r5, #\line_size
15:
    tst     r0, #4
    ldrne   r5, [r1], #4
    subne   r2, r2, #4
    strne   r5, [r0], #4
    tst     r0, #8
    ldmiane r1!, {r3, r5}
    subne   r2, r2, #8
    stmiane r0!, {r3, r5}
.if \write_align == 32 || \write_align == 64
    tst     r0, #16
    ldmiane r1!, {r3, r5-r7}
    subne   r2, r2, #16
    stmiane r0!, {r3, r5-r7}
.endif
.if \overfetch == 0
    /* Only preload if the source region extends into it. */
    cmp     r8, r4
    blt     18f
.endif
    pld     [r4]
    add     r4, r4, #\line_size
18:
.if \write_align == 64
    cmp     r2, #32
    blt     14b
    tst     r0, #32
    ldmiane r1!, {r3, r5-r11}
    subne   r2, r2, #32
    stmiane r0!, {r3, r5-r11}
.endif

    /* Optimize the branches based on the number of bytes left, */
    /* since we know that number of bytes left < line_size * 5. */
    /* ( < line_size * 3 if line_size is 64). */
.if \write_align == 64 || \line_size == 64
    cmp     r2, #64
.if \line_size == 64 && \write_align == 32
    add     r9, r1, #\preload_offset
    bic     r9, r9, #63
.if \preload_early == 1
    mov     r11, r9
.endif
    sub     r9, r9, r1
.else
    mov     r9, #\preload_offset
.if \preload_early == 1
    add     r11, r1, #\preload_offset
.endif
.endif
    blt     2b
.if \line_size == 64 && (\line_size * 3) <= (\preload_offset + 64)
    b       10b
.elseif \line_size == 32 && (\line_size * 5) <= (\preload_offset + 64)
    b       10b
.else
    cmp     r2, #(\preload_offset + 64)
    ble     10b
    b       19b
.endif
.elseif \write_align == 32
    /* In the case of write_align == 32 r9 will be equal to preload_offset. */
    cmp     r2, #32
    mov     r9, #\preload_offset
.if \preload_early == 1
    add     r11, r1, #\preload_offset
.endif
.if \line_size == 64 && (\line_size * 3) <= (\preload_offset + 32)
    bge     10b
    b       2b
.elseif \line_size == 32 && (\line_size * 5) <= (\preload_offset + 32)
    bge     10b
    b       2b
.else
    blt     2b
    cmp     r2, #(\preload_offset + 32)
    ble     10b
    b       19b
.endif
.else
    cmp     r2, #32
    add     r9, r1, #\preload_offset
    bic     r9, r9, #31
.if (\line_size * 5) <= (\preload_offset + 32)
    mov     r11, r9
    sub     r9, r9, r1
    bge     10b
    b       2b
.else
    /* If there are less than 32 bytes to go, skip all loops. */
    blt     2b
    cmp     r2, #(\preload_offset + 32)
    mov     r11, r9
    sub     r9, r9, r1
    /* If there are <= (preload_offset + 32) bytes to go, skip the main loop. */
    ble     10b
    b       19b
.endif
.endif
 .endm

#ifdef RPI_BEST_MEMCPY_ONLY

asm_function memcpy_armv5te_no_overfetch
    MEMCPY_VARIANT 32, 16, 16, 128, 1, 0
.endfunc

asm_function memcpy_armv5te_overfetch
    MEMCPY_VARIANT 32, 16, 16, 96, 1, 1
.endfunc

#else

asm_function memcpy_armv5te_no_overfetch_align_16_block_write_8_preload_96
    MEMCPY_VARIANT 32, 16, 8, 96, 0, 0
.endfunc

asm_function memcpy_armv5te_no_overfetch_align_16_block_write_16_preload_96
    MEMCPY_VARIANT 32, 16, 16, 96, 0, 0
.endfunc

asm_function memcpy_armv5te_no_overfetch_align_16_block_write_16_preload_early_96
    MEMCPY_VARIANT 32, 16, 16, 96, 1, 0
.endfunc

asm_function memcpy_armv5te_no_overfetch_align_16_block_write_16_preload_early_128
    MEMCPY_VARIANT 32, 16, 16, 128, 1, 0
.endfunc

asm_function memcpy_armv5te_no_overfetch_align_32_block_write_8_preload_96
    MEMCPY_VARIANT 32, 32, 8, 96, 0, 0
.endfunc

asm_function memcpy_armv5te_no_overfetch_align_32_block_write_16_preload_64
    MEMCPY_VARIANT 32, 32, 16, 64, 0, 0
.endfunc

asm_function memcpy_armv5te_no_overfetch_align_32_block_write_16_preload_96
    MEMCPY_VARIANT 32, 32, 16, 96, 0, 0
.endfunc

asm_function memcpy_armv5te_no_overfetch_align_32_block_write_16_preload_128
    MEMCPY_VARIANT 32, 32, 16, 128, 0, 0
.endfunc

asm_function memcpy_armv5te_no_overfetch_align_32_block_write_16_preload_160
    MEMCPY_VARIANT 32, 32, 16, 160, 0, 0
.endfunc

asm_function memcpy_armv5te_no_overfetch_align_32_block_write_16_preload_192
    MEMCPY_VARIANT 32, 32, 16, 192, 0, 0
.endfunc

asm_function memcpy_armv5te_no_overfetch_align_32_block_write_16_preload_256
    MEMCPY_VARIANT 32, 32, 16, 256, 0, 0
.endfunc

asm_function memcpy_armv5te_no_overfetch_align_32_block_write_32_preload_64
    MEMCPY_VARIANT 32, 32, 32, 64, 0, 0
.endfunc

asm_function memcpy_armv5te_no_overfetch_align_32_block_write_32_preload_96
    MEMCPY_VARIANT 32, 32, 32, 96, 0, 0
.endfunc

asm_function memcpy_armv5te_no_overfetch_align_32_block_write_32_preload_128
    MEMCPY_VARIANT 32, 32, 32, 128, 0, 0
.endfunc

asm_function memcpy_armv5te_no_overfetch_align_32_block_write_32_preload_160
    MEMCPY_VARIANT 32, 32, 32, 160, 0, 0
.endfunc

asm_function memcpy_armv5te_no_overfetch_align_32_block_write_32_preload_192
    MEMCPY_VARIANT 32, 32, 32, 192, 0, 0
.endfunc

asm_function memcpy_armv5te_no_overfetch_align_32_block_write_32_preload_256
    MEMCPY_VARIANT 32, 32, 32, 256, 0, 0
.endfunc

asm_function memcpy_armv5te_no_overfetch_align_32_block_write_16_preload_early_96
    MEMCPY_VARIANT 32, 32, 16, 96, 1, 0
.endfunc

asm_function memcpy_armv5te_no_overfetch_align_32_block_write_16_preload_early_128
    MEMCPY_VARIANT 32, 32, 16, 128, 1, 0
.endfunc

asm_function memcpy_armv5te_no_overfetch_align_32_block_write_16_preload_early_192
    MEMCPY_VARIANT 32, 32, 16, 192, 1, 0
.endfunc

asm_function memcpy_armv5te_no_overfetch_align_32_block_write_16_preload_early_256
    MEMCPY_VARIANT 32, 32, 16, 256, 1, 0
.endfunc

asm_function memcpy_armv5te_no_overfetch_align_32_block_write_32_preload_early_128
    MEMCPY_VARIANT 32, 32, 32, 128, 1, 0
.endfunc

asm_function memcpy_armv5te_no_overfetch_align_32_block_write_32_preload_early_192
    MEMCPY_VARIANT 32, 32, 32, 192, 1, 0
.endfunc

asm_function memcpy_armv5te_no_overfetch_align_32_block_write_32_preload_early_256
    MEMCPY_VARIANT 32, 32, 32, 256, 1, 0
.endfunc

asm_function memcpy_armv5te_no_overfetch_align_32_block_write_16_no_preload
    MEMCPY_VARIANT 32, 32, 16, 0, 0, 0
.endfunc

asm_function memcpy_armv5te_no_overfetch_align_32_block_write_32_no_preload
    MEMCPY_VARIANT 32, 32, 32, 0, 0, 0
.endfunc

asm_function memcpy_armv5te_no_overfetch_line_64_align_32_block_write_32_preload_early_128
    MEMCPY_VARIANT 64, 32, 32, 128, 1, 0
.endfunc

asm_function memcpy_armv5te_no_overfetch_line_64_align_32_block_write_32_preload_early_192
    MEMCPY_VARIANT 64, 32, 32, 192, 1, 0
.endfunc

asm_function memcpy_armv5te_no_overfetch_line_64_align_32_block_write_32_preload_early_256
    MEMCPY_VARIANT 64, 32, 32, 256, 1, 0
.endfunc

asm_function memcpy_armv5te_no_overfetch_line_64_align_32_block_write_32_preload_early_320
    MEMCPY_VARIANT 64, 32, 32, 320, 1, 0
.endfunc

asm_function memcpy_armv5te_no_overfetch_line_64_align_64_block_write_32_preload_early_256
    MEMCPY_VARIANT 64, 64, 32, 256, 1, 0
.endfunc

asm_function memcpy_armv5te_no_overfetch_line_64_align_64_block_write_32_preload_early_320
    MEMCPY_VARIANT 64, 64, 32, 320, 1, 0
.endfunc

/* Overfetching versions. */

asm_function memcpy_armv5te_overfetch_align_16_block_write_16_preload_early_128
    MEMCPY_VARIANT 32, 16, 16, 128, 1, 1
.endfunc

asm_function memcpy_armv5te_overfetch_align_32_block_write_32_preload_early_192
    MEMCPY_VARIANT 32, 32, 32, 192, 1, 1
.endfunc

#endif

#endif
