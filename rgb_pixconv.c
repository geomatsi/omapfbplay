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

#include "pixconv.h"

int rgb_open(const struct frame_format *ffmt,
        const struct frame_format *dfmt)
{
    printf("--> %s\n", __func__);
    return 0;
}

void rgb_convert(uint8_t *vdst[3], uint8_t *vsrc[3],
        uint8_t *pdst[3], uint8_t *psrc[3])
{
    printf("--> %s\n", __func__);
}

void rgb_finish(void)
{
    printf("--> %s\n", __func__);
}

void rgb_close(void)
{
    printf("--> %s\n", __func__);
}

PIXCONV(rgb) = {
    .name  = "rgb",
    .open  = rgb_open,
    .convert = rgb_convert,
    .finish  = rgb_finish,
    .close = rgb_close,
};
