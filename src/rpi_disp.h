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

#ifndef SUNXI_DISP_H
#define SUNXI_DISP_H

#include <inttypes.h>

#include "interfaces.h"

/*
 * Support for RPi hardware features.
 */
typedef struct {
    int                 fd_fb;
    int                 fd_disp;
    int                 fd_g2d;
    int                 fb_id;             /* /dev/fb0 = 0, /dev/fb1 = 1 */

    int                 xres, yres, bits_per_pixel;
    uint8_t            *framebuffer_addr;  /* mmapped address */
    uintptr_t           framebuffer_paddr; /* physical address */
    uint32_t            framebuffer_size;  /* total size of the framebuffer */
    int                 framebuffer_height;/* virtual vertical resolution */
    uint32_t            gfx_layer_size;    /* the size of the primary layer */

    uint8_t            *xserver_fbmem; /* framebuffer mapping done by xserver */

    /* Hardware cursor support */
    int                 cursor_enabled;
    int                 cursor_x, cursor_y;

    /* Layers support */
    int                 layer_id;
    int                 layer_has_scaler;

    /* Acelerated implementation of blt2d_i interface */
    blt2d_i             blt2d;
} rpi_disp_t;

rpi_disp_t *rpi_disp_init(const char *fb_device, void *xserver_fbmem);
int rpi_disp_close(rpi_disp_t *ctx);

#if 0

/*
 * Support for hardware cursor, which has 64x64 size, 2 bits per pixel,
 * four 32-bit ARGB entries in the palette.
 */
int sunxi_hw_cursor_load_64x64x2bpp(sunxi_disp_t *ctx, uint8_t pixeldata[1024]);
int sunxi_hw_cursor_load_32x32x8bpp(sunxi_disp_t *ctx, uint8_t pixeldata[1024]);
int sunxi_hw_cursor_load_palette(sunxi_disp_t *ctx, uint32_t *palette, int n);
int sunxi_hw_cursor_set_position(sunxi_disp_t *ctx, int x, int y);
int sunxi_hw_cursor_show(sunxi_disp_t *ctx);
int sunxi_hw_cursor_hide(sunxi_disp_t *ctx);

/*
 * Support for one sunxi disp layer (even though there are more than
 * one available) in the offscreen part of framebuffer, which may be
 * useful for DRI2 vsync aware frame flipping and implementing XV
 * extension (video overlay).
 */

int sunxi_layer_reserve(sunxi_disp_t *ctx);
int sunxi_layer_release(sunxi_disp_t *ctx);

int sunxi_layer_set_x8r8g8b8_input_buffer(sunxi_disp_t  *ctx,
                                          uint32_t      offset_in_framebuffer,
                                          int           width,
                                          int           height,
                                          int           stride);

int sunxi_layer_set_output_window(sunxi_disp_t *ctx, int x, int y, int w, int h);

int sunxi_layer_show(sunxi_disp_t *ctx);
int sunxi_layer_hide(sunxi_disp_t *ctx);

#endif

/*
 * Wait for vsync
 */
int rpi_wait_for_vsync(rpi_disp_t *ctx);

/* 
 * The following constants are used in rpi_disp.c and represent
 * the area threshold for falling back to CPU fill.
 */
#define RPI_FILL_SIZE_THRESHOLD_32BPP 5000
#define RPI_FILL_SIZE_THRESHOLD_16BPP 10000000

/* RPI counterpart for pixmanfill with the support for 16bpp and 32bpp */
int rpi_fill(void               *disp,
                   uint32_t           *bits,
                   int                 stride,
                   int                 bpp,
                   int                 x,
                   int                 y,
                   int                 width,
                   int                 height,
                   uint32_t            color);


/*
 * The following constants are used rpi_disp.c and represent
 * the area threshold below which the rpi_blit function will
 * return 0, indicating that a software blit is preferred. The
 * 16BPP constant applies to 16bpp to 16bpp blit.
 */
#define RPI_BLT_SIZE_THRESHOLD 1000
#define RPI_BLT_SIZE_THRESHOLD_16BPP 2500

/* RPI counterpart for pixman_blt with the support for 16bpp and 32bpp */
int rpi_blt(void               *disp,
                  uint32_t           *src_bits,
                  uint32_t           *dst_bits,
                  int                 src_stride,
                  int                 dst_stride,
                  int                 src_bpp,
                  int                 dst_bpp,
                  int                 src_x,
                  int                 src_y,
                  int                 dst_x,
                  int                 dst_y,
                  int                 w,
                  int                 h);

#endif
