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
 * Adapted for RPi: Copyright 2013 Harm Hanemaaijer <fgenfb@yahoo.com>
 *
 */

#include <stdlib.h>
#include <stdio.h> // For debugging.
#include <string.h>

#include "cpuinfo.h"
#include "cpu_backend.h"

/*
 * Threshold width, below which we fall to a more compact CPU blit function,
 * except for the case of rightwards overlapped blits, which are always
 * faster using the ARM-optimized functions defined here.
 */
#define ARM_BLT_WIDTH_THRESHOLD_32BPP 40
#define ARM_BLT_WIDTH_THRESHOLD_16BPP 60

#ifdef __arm__

static void writeback_scratch_to_mem_arm(int size, void *dst, const void *src);
static void aligned_fetch_fbmem_to_scratch_arm(int size, void *dst, const void *src);

/*
 * We use standard memcpy, which is only optimized for the aligned case on RPi
 * it seems.
 */

static void writeback_scratch_to_mem_arm(int size, void *dst, const void *src) {
    memcpy(dst, src, size);
}

/*
 * For this function, src and dst are 32-byte aligned.
 * ARM assembler implemention is available but doesn't work yet.
 */

#if 1

static void aligned_fetch_fbmem_to_scratch_arm(int size, void *dst, const void *src) {
    memcpy(dst, src, size);
}

#endif

#define SCRATCHSIZE 2048

/*
 * This is a function similar to memmove, which tries to minimize uncached read
 * penalty for the source buffer (for example if the source is a framebuffer).
 *
 * Note: because this implementation fetches data as 32 byte aligned chunks
 * valgrind is going to scream about read accesses outside the source buffer.
 * (even if an aligned 32 byte chunk contains only a single byte belonging
 * to the source buffer, the whole chunk is going to be read).
 */
static void
twopass_memmove_arm(void *dst_, const void *src_, size_t size)
{
    uint8_t tmpbuf[SCRATCHSIZE + 32 + 31];
    uint8_t *scratchbuf = (uint8_t *)((uintptr_t)(&tmpbuf[0] + 31) & ~31);
    uint8_t *dst = (uint8_t *)dst_;
    const uint8_t *src = (const uint8_t *)src_;
    uintptr_t alignshift = (uintptr_t)src & 31;
    uintptr_t extrasize = (alignshift == 0) ? 0 : 32;

    if (src > dst) {
        while (size >= SCRATCHSIZE) {
            aligned_fetch_fbmem_to_scratch_arm(SCRATCHSIZE + extrasize,
                                                scratchbuf, src - alignshift);
            writeback_scratch_to_mem_arm(SCRATCHSIZE, dst, scratchbuf + alignshift);
            size -= SCRATCHSIZE;
            dst += SCRATCHSIZE;
            src += SCRATCHSIZE;
        }
        if (size > 0) {
            aligned_fetch_fbmem_to_scratch_arm(size + extrasize,
                                                scratchbuf, src - alignshift);
            writeback_scratch_to_mem_arm(size, dst, scratchbuf + alignshift);
        }
    }
    else {
        uintptr_t remainder = size % SCRATCHSIZE;
        dst += size - remainder;
        src += size - remainder;
        size -= remainder;
        if (remainder) {
            aligned_fetch_fbmem_to_scratch_arm(remainder + extrasize,
                                                scratchbuf, src - alignshift);
            writeback_scratch_to_mem_arm(remainder, dst, scratchbuf + alignshift);
        }
        while (size > 0) {
            dst -= SCRATCHSIZE;
            src -= SCRATCHSIZE;
            size -= SCRATCHSIZE;
            aligned_fetch_fbmem_to_scratch_arm(SCRATCHSIZE + extrasize,
                                                scratchbuf, src - alignshift);
            writeback_scratch_to_mem_arm(SCRATCHSIZE, dst, scratchbuf + alignshift);
        }
    }
}

static void
twopass_blt_8bpp_arm(int        width,
                      int        height,
                      uint8_t   *dst_bytes,
                      uintptr_t  dst_stride,
                      uint8_t   *src_bytes,
                      uintptr_t  src_stride)
{
    if (src_bytes < dst_bytes + width &&
        src_bytes + src_stride * height > dst_bytes)
    {
        src_bytes += src_stride * height - src_stride;
        dst_bytes += dst_stride * height - dst_stride;
        dst_stride = -dst_stride;
        src_stride = -src_stride;
        if (src_bytes + width > dst_bytes)
        {
            while (--height >= 0)
            {
                twopass_memmove_arm(dst_bytes, src_bytes, width);
                dst_bytes += dst_stride;
                src_bytes += src_stride;
            }
            return;
        }
    }
    while (--height >= 0)
    {
        twopass_memmove_arm(dst_bytes, src_bytes, width);
        dst_bytes += dst_stride;
        src_bytes += src_stride;
    }
}

static int
overlapped_blt_arm(void     *self,
                    uint32_t *src_bits,
                    uint32_t *dst_bits,
                    int       src_stride,
                    int       dst_stride,
                    int       src_bpp,
                    int       dst_bpp,
                    int       src_x,
                    int       src_y,
                    int       dst_x,
                    int       dst_y,
                    int       width,
                    int       height)
{
    /*
     * Heuristic for falling back to more compact CPU blit; this tries to
     * catch the fact that for rightwards overlapped blits, overlapped_blt_arm
     * is almost always faster, even for small sizes.
     */
    if (((src_bpp == 16 && width < ARM_BLT_WIDTH_THRESHOLD_16BPP) ||
    (src_bpp == 32 && width < ARM_BLT_WIDTH_THRESHOLD_32BPP))
    && !(src_y == dst_y && src_x < dst_x && src_x + width >= dst_x))
        return 0;
    uint8_t *dst_bytes = (uint8_t *)dst_bits;
    uint8_t *src_bytes = (uint8_t *)src_bits;
    cpu_backend_t *ctx = (cpu_backend_t *)self;
    int bpp = src_bpp >> 3;
    int uncached_source = (src_bytes >= ctx->uncached_area_begin) &&
                          (src_bytes < ctx->uncached_area_end);
    if (!uncached_source)
        return 0;

    if (src_bpp != dst_bpp || src_bpp & 7 || src_stride < 0 || dst_stride < 0)
        return 0;

    twopass_blt_8bpp_arm((uintptr_t) width * bpp,
                          height,
                          dst_bytes + (uintptr_t) dst_y * dst_stride * 4 +
                                      (uintptr_t) dst_x * bpp,
                          (uintptr_t) dst_stride * 4,
                          src_bytes + (uintptr_t) src_y * src_stride * 4 +
                                      (uintptr_t) src_x * bpp,
                          (uintptr_t) src_stride * 4);
    return 1;
}

#endif

/* An empty, always failing implementation */
static int
overlapped_blt_noop(void     *self,
                    uint32_t *src_bits,
                    uint32_t *dst_bits,
                    int       src_stride,
                    int       dst_stride,
                    int       src_bpp,
                    int       dst_bpp,
                    int       src_x,
                    int       src_y,
                    int       dst_x,
                    int       dst_y,
                    int       width,
                    int       height)
{
    return 0;
}

/*
 * Source and destination have a different alignment within a 32-bit word (but are 16-bit aligned).
 * Source is 32-bit aligned, destination is 16-bit aligned. Common case at 16bpp.
 */

static int standard_source_word_aligned_blt(uint8_t *src, uint8_t * dst, int src_stride, int dst_stride,
int bpp, int alignshift_src, int alignshift_dst, int bw, int h) {
        int leftmost_short = alignshift_src & 2;
        int leftmost_words;
        if (alignshift_src != 0)
            leftmost_words = 8 - ((alignshift_src & 0x1B) >> 2);
        else
            leftmost_words = 0;
        bw -= leftmost_short + leftmost_words * 4;
        if (bw <= 0) {
            // Small width (within one chunk).
            leftmost_words -= (- bw) >> 2;
            int rightmost_short = (- bw) & 2;
            uint8_t *srclinep = src;
            uint8_t *dstlinep = dst;
            while (h > 0) {
                int offset = 0;
                if (leftmost_short) {
                    *(uint16_t *)dstlinep = *(uint16_t *)srclinep;
                    offset += 2;
                }
                for (int i = 0; i < leftmost_words; i++)
                    *(uint32_t *)(dstlinep + offset + i * 4) = *(uint32_t *)(srclinep + offset + i * 4);
                offset += leftmost_words * 4;
                if (rightmost_short)
                    *(uint16_t *)(dstlinep + offset) = *(uint16_t *)(srclinep + offset);
                srclinep += src_stride * 4;
                dstlinep += dst_stride * 4;
                h--;
            }
            return 1;
        }
        int chunks = bw >> 5;
        bw -= chunks << 5;
        int rightmost_words = bw >> 2;
        int rightmost_short = bw & 2;
        uint8_t *srclinep = src;
        uint8_t *dstlinep = dst;
        while (h > 0) {
            int offset = 0;
            uint32_t pix;
            /* The source is word-aligned, so the destination is not word aligned. */
                pix = *(uint32_t *)srclinep;
                *(uint16_t *)dstlinep = (uint16_t)pix;
                pix >>= 16;
                offset = 4;
                for (int i = 0; i < leftmost_words - 1; i++) {
                    uint32_t pix2 = *(uint32_t *)(srclinep + offset + i * 4);
                    *(uint32_t *)(dstlinep + offset - 2 + i * 4) = pix + (pix2 << 16);
                    pix = pix2 >> 16;
                }
                offset += leftmost_words * 4;
                for (int i = 0; i < chunks; i++) {
                     uint32_t pix2 = *(uint32_t *)(srclinep + offset);
                     *(uint32_t *)(dstlinep + offset - 2) = pix + (pix2 << 16);
                     pix = pix2 >> 16;
                     pix2 = *(uint32_t *)(srclinep + offset + 4);
                     *(uint32_t *)(dstlinep + offset + 4) = pix + (pix2 << 16);
                     pix = pix2 >> 16;
                     pix2 = *(uint32_t *)(srclinep + offset + 8);
                     *(uint32_t *)(dstlinep + offset + 6) = pix + (pix2 << 16);
                     pix = pix2 >> 16;
                     pix2 = *(uint32_t *)(srclinep + offset + 12);
                     *(uint32_t *)(dstlinep + offset + 10) = pix + (pix2 << 16);
                     pix = pix2 >> 16;
                     pix2 = *(uint32_t *)(srclinep + offset + 16);
                     *(uint32_t *)(dstlinep + offset + 14) = pix + (pix2 << 16);
                     pix = pix2 >> 16;
                     pix2 = *(uint32_t *)(srclinep + offset + 20);
                     *(uint32_t *)(dstlinep + offset + 18) = pix + (pix2 << 16);
                     pix = pix2 >> 16;
                     pix2 = *(uint32_t *)(srclinep + offset + 24);
                     *(uint32_t *)(dstlinep + offset + 22) = pix + (pix2 << 16);
                     pix = pix2 >> 16;
                     pix2 = *(uint32_t *)(srclinep + offset + 28);
                     *(uint32_t *)(dstlinep + offset + 26) = pix + (pix2 << 16);
                     pix = pix2 >> 16;
                     offset += 32;
                }
            for (int i = 0; i < rightmost_words; i++) {
                uint32_t pix2 = *(uint32_t *)(srclinep + offset + i * 4);
                *(uint32_t *)(dstlinep + offset - 2 + i *  4) = pix + (pix2 << 16);
                pix = pix2 >> 16;
            }
            offset += rightmost_words * 4;
            if (rightmost_short)
                *(uint32_t *)(dstlinep + offset - 2) = pix + (*(uint16_t *)(srclinep + offset) << 16);
            else
                *(uint16_t *)(dstlinep + offset - 2) = (uint16_t)pix;
            srclinep += src_stride * 4;
            dstlinep += dst_stride * 4;
            h--;
        }
        return 1;
}


/*
 * Source and destination have a different alignment within a 32-bit word (but are 16-bit aligned).
 * Source is 16-bit aligned, destination is 32-bit aligned. Common case at 16bpp.
 */

static int standard_destination_word_aligned_blt(uint8_t *src, uint8_t * dst, int src_stride, int dst_stride,
int bpp, int alignshift_src, int alignshift_dst, int bw, int h) {
        int leftmost_short = alignshift_src & 2;
        int leftmost_words;
        if (alignshift_src != 0)
            leftmost_words = 8 - ((alignshift_src & 0x1B) >> 2);
        else
            leftmost_words = 0;
        bw -= leftmost_short + leftmost_words * 4;
        if (bw <= 0) {
            // Small width (within one chunk).
            leftmost_words -= (- bw) >> 2;
            int rightmost_short = (- bw) & 2;
            uint8_t *srclinep = src;
            uint8_t *dstlinep = dst;
            while (h > 0) {
                int offset = 0;
                if (leftmost_short) {
                    *(uint16_t *)dstlinep = *(uint16_t *)srclinep;
                    offset += 2;
                }
                for (int i = 0; i < leftmost_words; i++)
                    *(uint32_t *)(dstlinep + offset + i * 4) = *(uint32_t *)(srclinep + offset + i * 4);
                offset += leftmost_words * 4;
                if (rightmost_short)
                    *(uint16_t *)(dstlinep + offset) = *(uint16_t *)(srclinep + offset);
                srclinep += src_stride * 4;
                dstlinep += dst_stride * 4;
                h--;
            }
            return 1;
        }
        int chunks = bw >> 5;
        bw -= chunks << 5;
        int rightmost_words = bw >> 2;
        int rightmost_short = bw & 2;
        uint8_t *srclinep = src;
        uint8_t *dstlinep = dst;
        while (h > 0) {
            int offset = 0;
            uint32_t pix;
            /* The source is aligned in the middle of a word, so the destination is word-aligned. */
            pix = *(uint16_t *)srclinep;
            offset = 2;
            for (int i = 0; i < leftmost_words; i++) {
                uint32_t pix2 = *(uint32_t *)(srclinep + offset + i * 4);
                *(uint32_t *)(dstlinep + offset - 2 + i *  4) = pix + (pix2 << 16);
                pix = pix2 >> 16;
            }
            offset += leftmost_words * 4;
            for (int i = 0; i < chunks; i++) {
                uint32_t pix2 = *(uint32_t *)(srclinep + offset);
                *(uint32_t *)(dstlinep + offset - 2) = pix + (pix2 << 16);
                pix = pix2 >> 16;
                pix2 = *(uint32_t *)(srclinep + offset + 4);
                *(uint32_t *)(dstlinep + offset + 4) = pix + (pix2 << 16);
                pix = pix2 >> 16;
                pix2 = *(uint32_t *)(srclinep + offset + 8);
                *(uint32_t *)(dstlinep + offset + 6) = pix + (pix2 << 16);
                pix = pix2 >> 16;
                pix2 = *(uint32_t *)(srclinep + offset + 12);
                *(uint32_t *)(dstlinep + offset + 10) = pix + (pix2 << 16);
                pix = pix2 >> 16;
                pix2 = *(uint32_t *)(srclinep + offset + 16);
                *(uint32_t *)(dstlinep + offset + 14) = pix + (pix2 << 16);
                pix = pix2 >> 16;
                pix2 = *(uint32_t *)(srclinep + offset + 20);
                *(uint32_t *)(dstlinep + offset + 18) = pix + (pix2 << 16);
                pix = pix2 >> 16;
                pix2 = *(uint32_t *)(srclinep + offset + 24);
                *(uint32_t *)(dstlinep + offset + 22) = pix + (pix2 << 16);
                pix = pix2 >> 16;
                pix2 = *(uint32_t *)(srclinep + offset + 28);
                *(uint32_t *)(dstlinep + offset + 26) = pix + (pix2 << 16);
                pix = pix2 >> 16;
                offset += 32;
            }
            for (int i = 0; i < rightmost_words; i++) {
                uint32_t pix2 = *(uint32_t *)(srclinep + offset + i * 4);
                *(uint32_t *)(dstlinep + offset - 2 + i *  4) = pix + (pix2 << 16);
                pix = pix2 >> 16;
            }
            offset += rightmost_words * 4;
            if (rightmost_short)
                *(uint32_t *)(dstlinep + offset - 2) = pix + (*(uint16_t *)(srclinep + offset) << 16);
            else
                *(uint16_t *)(dstlinep + offset - 2) = (uint16_t)pix;
            srclinep += src_stride * 4;
            dstlinep += dst_stride * 4;
            h--;
        }
        return 1;
}

/*
 * Source and destination have the same alignment within a 32-bit word.
 */

static int standard_word_aligned_blt(uint8_t *src, uint8_t * dst, int src_stride, int dst_stride,
int bpp, int alignshift_src, int alignshift_dst, int bw, int h) {\
        int leftmost_short = alignshift_src & 2;
        int leftmost_words;
        if (alignshift_src != 0)
            leftmost_words = 8 - ((alignshift_src & 0x1B) >> 2);
        else
            leftmost_words = 0;
        bw -= leftmost_short + leftmost_words * 4;
        if (bw <= 0) {
            // Small width (within one chunk).
            leftmost_words -= (- bw) >> 2;
            int rightmost_short = (- bw) & 2;
            uint8_t *srclinep = src;
            uint8_t *dstlinep = dst;
            while (h > 0) {
                int offset = 0;
                if (leftmost_short) {
                    *(uint16_t *)dstlinep = *(uint16_t *)srclinep;
                    offset += 2;
                }
                for (int i = 0; i < leftmost_words; i++)
                    *(uint32_t *)(dstlinep + offset + i * 4) = *(uint32_t *)(srclinep + offset + i * 4);
                offset += leftmost_words * 4;
                if (rightmost_short)
                    *(uint16_t *)(dstlinep + offset) = *(uint16_t *)(srclinep + offset);
                srclinep += src_stride * 4;
                dstlinep += dst_stride * 4;
                h--;
            }
            return 1;
        }
        int chunks = bw >> 5;
        bw -= chunks << 5;
        int rightmost_words = bw >> 2;
        int rightmost_short = bw & 2;
        uint8_t *srclinep = src;
        uint8_t *dstlinep = dst;
        while (h > 0) {
            int offset = 0;
            if (leftmost_short) {
                *(uint16_t *)dstlinep = *(uint16_t *)srclinep;
                offset += 2;
            }
            for (int i = 0; i < leftmost_words; i++)
                *(uint32_t *)(dstlinep + offset + i * 4) = *(uint32_t *)(srclinep + offset + i * 4);
            offset += leftmost_words * 4;
            for (int i = 0; i < chunks; i++) {
                double f0 = *(double *)(srclinep + offset);
                double f1 = *(double *)(srclinep + offset + 8);
                double f2 = *(double *)(srclinep + offset + 16);
                double f3 = *(double *)(srclinep + offset + 24);
                *(double *)(dstlinep + offset) = f0;
                *(double *)(dstlinep + offset + 8) = f1;
                *(double *)(dstlinep + offset + 16) = f2;
                *(double *)(dstlinep + offset + 24) = f3;
                offset += 32;
            }
            for (int i = 0; i < rightmost_words; i++)
                *(uint32_t *)(dstlinep + offset + i * 4) = *(uint32_t *)(srclinep + offset + i * 4);
            offset += rightmost_words * 4;
            if (rightmost_short)
                *(uint16_t *)(dstlinep + offset) = *(uint16_t *)(srclinep + offset);
            srclinep += src_stride * 4;
            dstlinep += dst_stride * 4;
            h--;
        }
        return 1;
}

/*
 * Optimized standard (non-screen source) blit function.
 * The "reversed" or "upsidedown" flags must not be set in the X driver.
 * Supports 16bpp and 32bpp: even widths >= 2.
 *
 * Divides into horizontal aligned 32-byte chunks.
 */

static int standard_blt(void     *self,
                          uint32_t *src_bits,
                          uint32_t *dst_bits,
                          int       src_stride,
                          int       dst_stride,
                          int       src_bpp,
                          int       dst_bpp,
                          int       src_x,
                          int       src_y,
                          int       dst_x,
                          int       dst_y,
                          int       w,
                          int       h)
{
    uint8_t *src = (uint8_t *)src_bits + src_y * src_stride * 4 + src_x * (src_bpp / 8);
    uint8_t *dst = (uint8_t *)dst_bits + dst_y * dst_stride * 4 + dst_x * (dst_bpp / 8);
    int bw = w * (src_bpp / 8);
    uintptr_t alignshift_src = (uintptr_t)src & 31;
    uintptr_t alignshift_dst = (uintptr_t)dst & 31;
    if (alignshift_src != alignshift_dst) {
        if ((alignshift_src & 3) == (alignshift_dst & 3))
            return standard_word_aligned_blt(src, dst, src_stride, dst_stride, src_bpp,
                alignshift_src, alignshift_dst, bw, h);
        else
            if ((alignshift_src & 3) == 0)
                return standard_source_word_aligned_blt(src, dst, src_stride, dst_stride, src_bpp,
                   alignshift_src, alignshift_dst, bw, h);
            else
                return standard_source_word_aligned_blt(src, dst, src_stride, dst_stride, src_bpp,
                   alignshift_src, alignshift_dst, bw, h);
    }
    /* Source and destination are aligned within a 32-byte chunk. */
        int leftmost_short = alignshift_src & 2;
        int leftmost_words;
        if (alignshift_src != 0)
            leftmost_words = 8 - ((alignshift_src & 0x1B) >> 2);
        else
            leftmost_words = 0;
        bw -= leftmost_short + leftmost_words * 4;
        if (bw <= 0) {
            // Small width (within one chunk).
            leftmost_words -= (- bw) >> 2;
            int rightmost_short = (- bw) & 2;
            uint8_t *srclinep = src;
            uint8_t *dstlinep = dst;
            while (h > 0) {
                int offset = 0;
                if (leftmost_short) {
                    *(uint16_t *)dstlinep = *(uint16_t *)srclinep;
                    offset += 2;
                }
                for (int i = 0; i < leftmost_words; i++)
                    *(uint32_t *)(dstlinep + offset + i * 4) = *(uint32_t *)(srclinep + offset + i * 4);
                offset += leftmost_words * 4;
                if (rightmost_short)
                    *(uint16_t *)(dstlinep + offset) = *(uint16_t *)(srclinep + offset);
                srclinep += src_stride * 4;
                dstlinep += dst_stride * 4;
                h--;
            }
            return 1;
        }
        int chunks = bw >> 5;
        bw -= chunks << 5;
        int rightmost_words = bw >> 2;
        int rightmost_short = bw & 2;
        uint8_t *srclinep = src;
        uint8_t *dstlinep = dst;
        while (h > 0) {
            int offset = 0;
            if (leftmost_short) {
                *(uint16_t *)dstlinep = *(uint16_t *)srclinep;
                offset += 2;
            }
            for (int i = 0; i < leftmost_words; i++)
                *(uint32_t *)(dstlinep + offset + i * 4) = *(uint32_t *)(srclinep + offset + i * 4);
            offset += leftmost_words * 4;
            for (int i = 0; i < chunks; i++) {
                double f0 = *(double *)(srclinep + offset);
                double f1 = *(double *)(srclinep + offset + 8);
                double f2 = *(double *)(srclinep + offset + 16);
                double f3 = *(double *)(srclinep + offset + 24);
                *(double *)(dstlinep + offset) = f0;
                *(double *)(dstlinep + offset + 8) = f1;
                *(double *)(dstlinep + offset + 16) = f2;
                *(double *)(dstlinep + offset + 24) = f3;
                offset += 32;
            }
            for (int i = 0; i < rightmost_words; i++)
                *(uint32_t *)(dstlinep + offset + i * 4) = *(uint32_t *)(srclinep + offset + i * 4);
            offset += rightmost_words * 4;
            if (rightmost_short)
                *(uint16_t *)(dstlinep + offset) = *(uint16_t *)(srclinep + offset);
            srclinep += src_stride * 4;
            dstlinep += dst_stride * 4;
            h--;
        }
        return 1;
}

static int
fill_noop(void                *self,
          uint32_t            *bits,
          int                 stride,
          int                 bpp,
          int                 x,
          int                 y,
          int                 width,
          int                 height,
          uint32_t            color)
{
    return 0;
}


cpu_backend_t *cpu_backend_init(uint8_t *uncached_buffer,
                                size_t   uncached_buffer_size)
{
    cpu_backend_t *ctx = calloc(sizeof(cpu_backend_t), 1);
    if (!ctx)
        return NULL;

    ctx->uncached_area_begin = uncached_buffer;
    ctx->uncached_area_end   = uncached_buffer + uncached_buffer_size;

    ctx->blt2d.self = ctx;
    ctx->blt2d.overlapped_blt = overlapped_blt_noop;
    /*
     * Initialize the fill function with NULL to indicate that is not
     * available.
     */
    ctx->blt2d.fill = NULL;

    ctx->cpuinfo = cpuinfo_init();

#ifdef __arm__
#if 0
    if (ctx->cpuinfo->has_arm_neon) {
        ctx->blt2d.overlapped_blt = overlapped_blt_neon;
    }
#endif
    ctx->blt2d.overlapped_blt = overlapped_blt_arm;
    ctx->blt2d.standard_blt = NULL; /* standard_blt; */
#endif

    return ctx;
}

void cpu_backend_close(cpu_backend_t *ctx)
{
    if (ctx->cpuinfo)
        cpuinfo_close(ctx->cpuinfo);

    free(ctx);
}
