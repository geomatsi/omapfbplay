/*
    Copyright (C) 2009 Mans Rullgard

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
#include <stdlib.h>
#include <stdint.h>

#include "frame.h"
#include "memman.h"
#include "util.h"

static uint8_t *frame_buf;

static int
sysmem_alloc_frames(struct frame_format *ff, unsigned bufsize,
                    struct frame **fr, unsigned *nf)
{
    int buf_w = ff->width, buf_h = ff->height;
    struct frame *frames;
    unsigned num_frames;
    unsigned frame_size;
    void *fbp;
    int i;

    frame_size = buf_w * buf_h * 3 / 2;
    num_frames = MAX(bufsize / frame_size, MIN_FRAMES);
    bufsize = num_frames * frame_size;

    fprintf(stderr, "Using %d frame buffers, frame_size=%d\n",
            num_frames, frame_size);

    if (posix_memalign(&fbp, 16, bufsize)) {
        fprintf(stderr, "Error allocating frame buffers: %d bytes\n", bufsize);
        return -1;
    }

    frame_buf = fbp;
    frames = calloc(num_frames, sizeof(*frames));

    for (i = 0; i < num_frames; i++) {
        uint8_t *p = frame_buf + i * frame_size;

        frames[i].virt[0] = p;
        frames[i].virt[1] = p + buf_w * buf_h;
        frames[i].virt[2] = frames[i].virt[1] + buf_w / 2;
        frames[i].linesize[0] = ff->width;
        frames[i].linesize[1] = ff->width;
        frames[i].linesize[2] = ff->width;
    }

    ff->y_stride  = ff->width;
    ff->uv_stride = ff->width;

    *fr = frames;
    *nf = num_frames;

    return 0;
}

static void
sysmem_free_frames(struct frame *frames, unsigned nf)
{
    free(frame_buf);
    frame_buf = NULL;
}

DRIVER(memman, sysmem) = {
    .name         = "system",
    .alloc_frames = sysmem_alloc_frames,
    .free_frames  = sysmem_free_frames,
};
