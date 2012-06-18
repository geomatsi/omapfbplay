/*
    Copyright (C) 2009 Mans Rullgard
    Copyright (C) 2012 matsi

    Permission is hereby granted, free of charge, to any person
    obtaining a copy of this software and associated documentation
    files (the "Software"), to deal in the Software without
    restriction, including without limitation the rights to use, copy,
    modify, merge, publish, distribute, sublicense, and/or sell copies
    of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
    HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
    WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>

#include "pixconv.h"
#include "pixfmt.h"
#include "frame.h"

static const struct pixfmt *pfmt;
static struct frame_format ffmt;
static struct frame_format dfmt;

int rgb_open(const struct frame_format *ff,
        const struct frame_format *df)
{
    if (df->pixfmt != PIX_FMT_RGBA) {
        fprintf(stderr, "Unknown dst pixel format %d\n", df->pixfmt);
        return -1;
    }

    ffmt = *ff;
	dfmt = *df;

    pfmt = ofbp_get_pixfmt(ffmt.pixfmt);

    if (!pfmt) {
        fprintf(stderr, "Unknown src pixel format %d\n", ffmt.pixfmt);
        return -1;
    }

    return 0;
}

void rgb_finish(void)
{
}

void rgb_close(void)
{
}

void rgb_convert(uint8_t *vdst[3], uint8_t *vsrc[3],
        uint8_t *pdst[3], uint8_t *psrc[3])
{
	struct frame *f = (struct frame *) vsrc;
    uint8_t *image = (uint8_t *) vdst;

    uint32_t hh = (uint32_t) dfmt.height;
    uint32_t ww = (uint32_t) dfmt.width;

    uint8_t *yp, *up, *vp;
    uint8_t y, u, v;
    uint8_t r, g, b;
    int xx, yy;

    yp = f->virt[pfmt->plane[0]] + pfmt->start[0];
    up = f->virt[pfmt->plane[1]] + pfmt->start[1];
    vp = f->virt[pfmt->plane[2]] + pfmt->start[2];

#if 0
    for (yy = 0; yy < ffmt.height; yy++) {
        for (xx = 0; xx < ffmt.width; xx++) {
#else
    for (yy = 0; yy < hh; yy++) {
        for (xx = 0; xx < ww; xx++) {
#endif
            y = yp[(yy >> pfmt->vsub[0])*(f->linesize[0]) + (xx >> pfmt->hsub[0])];
            u = up[(yy >> pfmt->vsub[1])*(f->linesize[1]) + (xx >> pfmt->hsub[1])];
            v = vp[(yy >> pfmt->vsub[2])*(f->linesize[2]) + (xx >> pfmt->hsub[2])];

            r = y + 1.402*(v-128);
            g = y - 0.34414*(u-128) - 0.71414*(v-128);
            b = y + 1.772*(u-128);

#if 0
            *(image + 4*(yy*ffmt.width + xx) + 0) = r;
            *(image + 4*(yy*ffmt.width + xx) + 1) = g;
            *(image + 4*(yy*ffmt.width + xx) + 2) = b;
            *(image + 4*(yy*ffmt.width + xx) + 3) = 128;
#else
            *(image + 4*(yy*ww + xx) + 0) = r;
            *(image + 4*(yy*ww + xx) + 1) = g;
            *(image + 4*(yy*ww + xx) + 2) = b;
            *(image + 4*(yy*ww + xx) + 3) = 128;
#endif
        }
    }
}

PIXCONV(rgb) = {
    .name  = "rgb",
    .open  = rgb_open,
    .convert = rgb_convert,
    .finish  = rgb_finish,
    .close = rgb_close,
};
