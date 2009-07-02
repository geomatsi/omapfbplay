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

#ifndef OFB_DISPLAY_H
#define OFB_DISPLAY_H

#include <stdint.h>

struct frame {
    uint8_t *data[3];
    int linesize;
    int pic_num;
    int next;
    int prev;
    int refs;
};

#define ALIGN(n, a) (((n)+((a)-1))&~((a)-1))
#define MIN(a, b) ((a) < (b)? (a): (b))

#define OFB_FULLSCREEN 1
#define OFB_DOUBLE_BUF 2

int display_open(const char *name, unsigned w, unsigned h, unsigned flags);
void display_frame(struct frame *f);
void display_close(void);

void yuv420_to_yuv422(uint8_t *yuv, uint8_t *y, uint8_t *u, uint8_t *v,
                      int w, int h, int yw, int cw, int dw);

#endif /* OFB_DISPLAY_H */