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
 * Copyright 2013 Harm Hanemaaijer <fgenfb@yahoo.com>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <pixman.h>

#include "xorgVersion.h"
#include "xf86_OSproc.h"
#include "xf86.h"
#include "xf86drm.h"
#include "dri2.h"
#include "damage.h"
#include "fb.h"

#include "fbdev_priv.h"
#include "rpi_x.h"

/*
 * If USE_STANDARD_BLT is defined, use the standard_blt function from the
 * device-independent interface instead of pixman. Pixman is significantly
 * faster at the moment.
 */

/* #define USE_STANDARD_BLT */

/*
 * The code below is borrowed from "xserver/fb/fbwindow.c"
 */

static void
xCopyWindowProc(DrawablePtr pSrcDrawable,
                 DrawablePtr pDstDrawable,
                 GCPtr pGC,
                 BoxPtr pbox,
                 int nbox,
                 int dx,
                 int dy,
                 Bool reverse, Bool upsidedown, Pixel bitplane, void *closure)
{
    FbBits *src;
    FbStride srcStride;
    int srcBpp;
    int srcXoff, srcYoff;
    FbBits *dst;
    FbStride dstStride;
    int dstBpp;
    int dstXoff, dstYoff;
    ScreenPtr pScreen = pDstDrawable->pScreen;
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    RPIAccel *private = RPI_ACCEL(pScrn);

    fbGetDrawable(pSrcDrawable, src, srcStride, srcBpp, srcXoff, srcYoff);
    fbGetDrawable(pDstDrawable, dst, dstStride, dstBpp, dstXoff, dstYoff);

    while (nbox--) {
        int w = pbox->x2 - pbox->x1;
        int h = pbox->y2 - pbox->y1;
        Bool done;
        done = private->blt2d_overlapped_blt(private->blt2d_self,
                                           (uint32_t *)src, (uint32_t *)dst,
                                           srcStride, dstStride,
                                           srcBpp, dstBpp, (pbox->x1 + dx + srcXoff),
                                           (pbox->y1 + dy + srcYoff), (pbox->x1 + dstXoff),
                                           (pbox->y1 + dstYoff), w,
                                           h);
        /* When using acceleration, try the ARM CPU back end as fallback. */
        if (!done) {
            if (private->blt2d_cpu_backend != NULL)
                done = private->blt2d_cpu_backend->overlapped_blt(
                             private->blt2d_cpu_backend->self,
                             (uint32_t *)src, (uint32_t *)dst,
                             srcStride, dstStride,
                             srcBpp, dstBpp, (pbox->x1 + dx + srcXoff),
                             (pbox->y1 + dy + srcYoff), (pbox->x1 + dstXoff),
                             (pbox->y1 + dstYoff), w,
                             h);
            if (!done)
                /* fallback to fbBlt */
                fbBlt(src + (pbox->y1 + dy + srcYoff) * srcStride,
                  srcStride,
                  (pbox->x1 + dx + srcXoff) * srcBpp,
                  dst + (pbox->y1 + dstYoff) * dstStride,
                  dstStride,
                  (pbox->x1 + dstXoff) * dstBpp,
                  w * dstBpp,
                  h,
                  GXcopy, FB_ALLONES, dstBpp, reverse, upsidedown);
        }
        pbox++;
    }

    fbFinishAccess(pDstDrawable);
    fbFinishAccess(pSrcDrawable);
}

static void
xCopyWindow(WindowPtr pWin, DDXPointRec ptOldOrg, RegionPtr prgnSrc)
{
    RegionRec rgnDst;
    int dx, dy;

    PixmapPtr pPixmap = fbGetWindowPixmap(pWin);
    DrawablePtr pDrawable = &pPixmap->drawable;

    dx = ptOldOrg.x - pWin->drawable.x;
    dy = ptOldOrg.y - pWin->drawable.y;
    RegionTranslate(prgnSrc, -dx, -dy);

    RegionNull(&rgnDst);

    RegionIntersect(&rgnDst, &pWin->borderClip, prgnSrc);

#ifdef COMPOSITE
    if (pPixmap->screen_x || pPixmap->screen_y)
        RegionTranslate(&rgnDst, -pPixmap->screen_x, -pPixmap->screen_y);
#endif

    miCopyRegion(pDrawable, pDrawable,
                 0, &rgnDst, dx, dy, xCopyWindowProc, 0, 0);

    RegionUninit(&rgnDst);
    fbValidateDrawable(&pWin->drawable);
}

/*****************************************************************************/

static void
xCopyNtoN(DrawablePtr pSrcDrawable,
          DrawablePtr pDstDrawable,
          GCPtr pGC,
          BoxPtr pbox,
          int nbox,
          int dx,
          int dy,
          Bool reverse, Bool upsidedown, Pixel bitplane, void *closure)
{
    FbBits *src;
    FbStride srcStride;
    int srcBpp;
    int srcXoff, srcYoff;
    FbBits *dst;
    FbStride dstStride;
    int dstBpp;
    int dstXoff, dstYoff;
    ScreenPtr pScreen = pDstDrawable->pScreen;
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    RPIAccel *private = RPI_ACCEL(pScrn);
    Bool try_pixman, try_standard_blt;

    fbGetDrawable(pSrcDrawable, src, srcStride, srcBpp, srcXoff, srcYoff);
    fbGetDrawable(pDstDrawable, dst, dstStride, dstBpp, dstXoff, dstYoff);

#ifdef USE_STANDARD_BLT
    if (!reverse && !upsidedown && private->blt2d_standard_blt != NULL)
        try_standard_blt= TRUE;
    else
        try_standard_blt = FALSE;
#else
    if (!reverse && !upsidedown)
        try_pixman = TRUE;
    else
        try_pixman = FALSE;
#endif

    while (nbox--) {
        /*
         * The following scenarios exist regarding accelerated blits:
         * 1. Use hardware blit and the ARM CPU back-end as fall back.
         *    private->blt2d_overlapped_blt is the accelerated blit function.
         *    private->btl2d_cpu_back_end is initialized with the CPU back-end.
         * 2. Use hardware blit and not use the ARM CPU back-end as fall back.
         *    private->blt2d_overlapped_blt is the hardware blit function.
         *    private->blt2d_cpu_back_end is NULL.
         * 3. Use the ARM CPU back-end only.
         *    private->blt2d_overlapped_blt is the ARM CPU back-end blit function.
         *    private->blt2d_cpu_back_end is NULL.
         */
        int w = pbox->x2 - pbox->x1;
        int h = pbox->y2 - pbox->y1;
        Bool done;
        done = private->blt2d_overlapped_blt(
                             private->blt2d_self,
                             (uint32_t *)src, (uint32_t *)dst,
                             srcStride, dstStride,
                             srcBpp, dstBpp, (pbox->x1 + dx + srcXoff),
                             (pbox->y1 + dy + srcYoff), (pbox->x1 + dstXoff),
                             (pbox->y1 + dstYoff), w,
                             h);
        if (!done) {
            /* When using acceleration, try the ARM CPU back end as fallback. */
            if (private->blt2d_cpu_backend != NULL)
                done = private->blt2d_cpu_backend->overlapped_blt(
                             private->blt2d_cpu_backend->self,
                             (uint32_t *)src, (uint32_t *)dst,
                             srcStride, dstStride,
                             srcBpp, dstBpp, (pbox->x1 + dx + srcXoff),
                             (pbox->y1 + dy + srcYoff), (pbox->x1 + dstXoff),
                             (pbox->y1 + dstYoff), w,
                             h);

            if (!done) {
                /* then standard_blt or pixman */
#ifdef USE_STANDARD_BLT
                if (try_standard_blt)
                    done = private->blt2d_standard_blt(
                        private->blt2d_self,
                        (uint32_t *)src, (uint32_t *)dst, srcStride, dstStride,
                        srcBpp, dstBpp, (pbox->x1 + dx + srcXoff),
                        (pbox->y1 + dy + srcYoff), (pbox->x1 + dstXoff),
                        (pbox->y1 + dstYoff), w,
                        h);
#else
                if (try_pixman)
                    done = pixman_blt((uint32_t *)src, (uint32_t *)dst, srcStride, dstStride,
                        srcBpp, dstBpp, (pbox->x1 + dx + srcXoff),
                        (pbox->y1 + dy + srcYoff), (pbox->x1 + dstXoff),
                        (pbox->y1 + dstYoff), w,
                        h);
#endif

                /* fallback to fbBlt if other methods did not work */
                if (!done)
                    // Due to the check in xCopyArea, it is guaranteed that pGC->alu == GXcopy
                    // and the planemask is FB_ALLONES.
                    fbBlt(src + (pbox->y1 + dy + srcYoff) * srcStride,
                        srcStride,
                        (pbox->x1 + dx + srcXoff) * srcBpp,
                        dst + (pbox->y1 + dstYoff) * dstStride,
                        dstStride,
                        (pbox->x1 + dstXoff) * dstBpp,
                        w * dstBpp,
                        h, GXcopy, FB_ALLONES, dstBpp, reverse, upsidedown);
            }
        }
        pbox++;
    }

    fbFinishAccess(pDstDrawable);
    fbFinishAccess(pSrcDrawable);
}

static RegionPtr
xCopyArea(DrawablePtr pSrcDrawable,
         DrawablePtr pDstDrawable,
         GCPtr pGC,
         int xIn, int yIn, int widthSrc, int heightSrc, int xOut, int yOut)
{
    CARD8 alu = pGC ? pGC->alu : GXcopy;
    FbBits pm = pGC ? fbGetGCPrivate(pGC)->pm : FB_ALLONES;

    if (pm == FB_ALLONES && alu == GXcopy && 
        pSrcDrawable->bitsPerPixel == pDstDrawable->bitsPerPixel &&
        (pSrcDrawable->bitsPerPixel == 32 || pSrcDrawable->bitsPerPixel == 16))
    {
        return miDoCopy(pSrcDrawable, pDstDrawable, pGC, xIn, yIn,
                    widthSrc, heightSrc, xOut, yOut, xCopyNtoN, 0, 0);
    }
    return fbCopyArea(pSrcDrawable,
                      pDstDrawable,
                      pGC,
                      xIn, yIn, widthSrc, heightSrc, xOut, yOut);
}

/*
 * The following function is adapted from xserver/fb/fbPutImage.c.
 */

void xPutImage(DrawablePtr pDrawable,
           GCPtr pGC,
           int depth,
           int x, int y, int w, int h, int leftPad, int format, char *pImage)
{
    FbGCPrivPtr pPriv;

    FbStride srcStride;
    FbStip *src;
    RegionPtr pClip;
    FbStip *dst;
    FbStride dstStride;
    int dstBpp;
    int dstXoff, dstYoff;
    int nbox;
    BoxPtr pbox;
    int x1, y1, x2, y2;

    if (format == XYBitmap || format == XYPixmap ||
    pDrawable->bitsPerPixel != BitsPerPixel(pDrawable->depth)) {
        fbPutImage(pDrawable, pGC, depth, x, y, w, h, leftPad, format, pImage);
        return;
    }

    pPriv =fbGetGCPrivate(pGC);
    if (pPriv->pm != FB_ALLONES || pGC->alu != GXcopy) {
        fbPutImage(pDrawable, pGC, depth, x, y, w, h, leftPad, format, pImage);
        return;
    }

    ScreenPtr pScreen = pDrawable->pScreen;
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    RPIAccel *private = RPI_ACCEL(pScrn);

    src = (FbStip *)pImage;

    x += pDrawable->x;
    y += pDrawable->y;

    srcStride = PixmapBytePad(w, pDrawable->depth) / sizeof(FbStip);
    pClip = fbGetCompositeClip(pGC);

    fbGetStipDrawable(pDrawable, dst, dstStride, dstBpp, dstXoff, dstYoff);

    for (nbox = RegionNumRects(pClip),
        pbox = RegionRects(pClip); nbox--; pbox++) {
        x1 = x;
        y1 = y;
        x2 = x + w;
        y2 = y + h;
        if (x1 < pbox->x1)
            x1 = pbox->x1;
        if (y1 < pbox->y1)
            y1 = pbox->y1;
        if (x2 > pbox->x2)
            x2 = pbox->x2;
        if (y2 > pbox->y2)
            y2 = pbox->y2;
        if (x1 >= x2 || y1 >= y2)
            continue;
        Bool done = FALSE;
        int w = x2 - x1;
        int h = y2 - y1;
        /* first try pixman (ARM) */
        if (!done)
#ifdef USE_STANDARD_BLT
            if (private->blt2d_standard_blt != NULL)
                done = private->blt2d_standard_blt(
                    private->blt2d_self,
                    (uint32_t *)src, (uint32_t *)dst, srcStride, dstStride,
                    dstBpp, dstBpp, x1 - x,
                    y1 - y, x1 + dstXoff,
                    y1 + dstYoff, w,
                    h);
#else
            done = pixman_blt((uint32_t *)src, (uint32_t *)dst, srcStride, dstStride,
                 dstBpp, dstBpp, x1 - x,
                 y1 - y, x1 + dstXoff,
                 y1 + dstYoff, w,
                 h);
#endif
        // otherwise fall back to fb */
        if (!done)
            fbBlt(src + (y1 - y) * srcStride,
                  srcStride,
                  (x1 - x) * dstBpp,
                  dst + (y1 + dstYoff) * dstStride,
                  dstStride,
                  (x1 + dstXoff) * dstBpp,
                  w * dstBpp,
                  h, GXcopy, FB_ALLONES, dstBpp, FALSE, FALSE);
    }
    fbFinishAccess(pDrawable);
}

/* Adapted from fbPolyFillRect and fbFill. */

static void xPolyFillRect(DrawablePtr pDrawable,
                          GCPtr pGC,
                          int nrect,
                          xRectangle * prect)
{
    ScreenPtr pScreen;
    ScrnInfoPtr pScrn;
    RegionPtr pClip;
    BoxPtr pbox;
    BoxPtr pextent;
    int extentX1, extentX2, extentY1, extentY2;
    int fullX1, fullX2, fullY1, fullY2;
    int partX1, partX2, partY1, partY2;
    int n;
    FbBits *dst;
    int dstStride;
    int dstBpp;
    int dstXoff, dstYoff;
    int xorg, yorg;
    FbGCPrivPtr pPriv;
    Bool outside_framebuffer;
    pPriv = fbGetGCPrivate(pGC);
    FbBits pm = pPriv->pm;
    Bool try_blt2d_fill, try_pixman_fill;

    if (pGC->fillStyle != FillSolid || pm != FB_ALLONES || pPriv->and) {
        fbPolyFillRect(pDrawable, pGC, nrect, prect);
        return;
    }

    pScreen = pDrawable->pScreen;
    pScrn = xf86Screens[pScreen->myNum];
    RPIAccel *private = RPI_ACCEL(pScrn);
    pClip = fbGetCompositeClip(pGC);

    // Note: dstXoff and dstYoff are generally zero or negative.
    fbGetDrawable(pDrawable, dst, dstStride, dstBpp, dstXoff, dstYoff);

    xorg = pDrawable->x;
    yorg = pDrawable->y;

    if (!pPriv->and) {
        if (private->blt2d_fill != NULL)
            try_blt2d_fill = TRUE;
        else
            try_blt2d_fill = FALSE;
        try_pixman_fill = TRUE;
    }
    else {
        try_blt2d_fill = FALSE;
        try_pixman_fill = FALSE;
    }

    pextent = REGION_EXTENTS(pGC->pScreen, pClip);
    extentX1 = pextent->x1;
    extentY1 = pextent->y1;
    extentX2 = pextent->x2;
    extentY2 = pextent->y2;
    while (nrect--)
    {
        fullX1 = prect->x + xorg;
        fullY1 = prect->y + yorg;
        fullX2 = fullX1 + (int) prect->width;
        fullY2 = fullY1 + (int) prect->height;
        prect++;

        if (fullX1 < extentX1)
            fullX1 = extentX1;

        if (fullY1 < extentY1)
            fullY1 = extentY1;

         if (fullX2 > extentX2)
            fullX2 = extentX2;

        if (fullY2 > extentY2)
            fullY2 = extentY2;

        if ((fullX1 >= fullX2) || (fullY1 >= fullY2))
            continue;
        n = REGION_NUM_RECTS (pClip);
        if (n == 1)
        {
            Bool done = FALSE;
            int x ,y, w, h;
            x = fullX1 + dstXoff;
            y = fullY1 + dstYoff;
            w = fullX2 - fullX1;
            h = fullY2 - fullY1;
            if (try_blt2d_fill)
                done = private->blt2d_fill(private->blt2d_self, (uint32_t *)dst, dstStride, dstBpp, x, y, w, h, pPriv->xor);
            if (!done) {
                if (try_pixman_fill)
                    done = pixman_fill((uint32_t *)dst, dstStride, dstBpp, x, y, w, h, pPriv->xor);
                if (!done)
                    fbSolid(dst + y * dstStride, dstStride, x * dstBpp, dstBpp, w * dstBpp, h, pPriv->and, pPriv->xor);
            }
        }
        else
        {
            pbox = REGION_RECTS(pClip);
            /*
             * clip the rectangle to each box in the clip region
             * this is logically equivalent to calling Intersect()
             */
            while(n--)
            {
                partX1 = pbox->x1;
                if (partX1 < fullX1)
                    partX1 = fullX1;
                partY1 = pbox->y1;
                if (partY1 < fullY1)
                    partY1 = fullY1;
                partX2 = pbox->x2;
                if (partX2 > fullX2)
                    partX2 = fullX2;
                partY2 = pbox->y2;
                if (partY2 > fullY2)
                    partY2 = fullY2;

                pbox++;

                if (partX1 < partX2 && partY1 < partY2) {
                    Bool done = FALSE;
                    int w, h;
                    int x = partX1 + dstXoff;
                    int y = partY1 + dstYoff;
                    w = partX2 - partX1;
                    h = partY2 - partY1;
                    if (try_blt2d_fill)
                        done = private->blt2d_fill(private->blt2d_self, (uint32_t *)dst, dstStride, dstBpp, x, y, w, h, pPriv->xor);
                    if (!done) {
                        if (try_pixman_fill)
                            done = pixman_fill((uint32_t *)dst, dstStride, dstBpp, x, y, w, h, pPriv->xor);
                        if (!done)
                            fbSolid(dst + y * dstStride, dstStride, x * dstBpp, dstBpp, w * dstBpp, h, pPriv->and, pPriv->xor);
                    }
                }
            }
        }
    }
    fbFinishAccess(pDrawable);
}

static Bool
xCreateGC(GCPtr pGC)
{
    ScreenPtr pScreen = pGC->pScreen;
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    RPIAccel *self = RPI_ACCEL(pScrn);
    Bool result;

    if (!fbCreateGC(pGC))
        return FALSE;

    if (!self->pGCOps) {
        self->pGCOps = calloc(1, sizeof(GCOps));
        memcpy(self->pGCOps, pGC->ops, sizeof(GCOps));

        /* Add our own hook for CopyArea function */
        self->pGCOps->CopyArea = xCopyArea;
        /* Add our own hook for PutImage */
        self->pGCOps->PutImage = xPutImage;
        /* Add our own hook for PolyFillRect */
        self->pGCOps->PolyFillRect = xPolyFillRect;
    }
    pGC->ops = self->pGCOps;

    return TRUE;
}

/*****************************************************************************/

RPIAccel *RPIAccel_Init(ScreenPtr pScreen, blt2d_i *blt2d, blt2d_i *blt2d_cpu_backend)
{
    RPIAccel *private = calloc(1, sizeof(RPIAccel));
    if (!private) {
        xf86DrvMsg(pScreen->myNum, X_INFO,
            "RPIAccel_Init: calloc failed\n");
        return NULL;
    }

    /* Cache the pointers from blt2d_i here */
    private->blt2d_self = blt2d->self;
    private->blt2d_cpu_backend = blt2d_cpu_backend;
    private->blt2d_overlapped_blt = blt2d->overlapped_blt;
    private->blt2d_standard_blt = blt2d->standard_blt;
    private->blt2d_fill = blt2d->fill;

    /* Wrap the current CopyWindow function */
    private->CopyWindow = pScreen->CopyWindow;
    pScreen->CopyWindow = xCopyWindow;

    /* Wrap the current CreateGC function */
    private->CreateGC = pScreen->CreateGC;
    pScreen->CreateGC = xCreateGC;

    return private;
}

void RPIAccel_Close(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    RPIAccel *private = RPI_ACCEL(pScrn);

    pScreen->CopyWindow = private->CopyWindow;
    pScreen->CreateGC   = private->CreateGC;

    if (private->pGCOps) {
        free(private->pGCOps);
    }
}
