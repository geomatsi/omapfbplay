/*
    Copyright (C) 2008 Mans Rullgard

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
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>

#include <linux/fb.h>
#include <linux/omapfb.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

#define BUFFER_SIZE (64*1024*1024)

extern void yuv420_to_yuv422(uint8_t *yuv, uint8_t *y, uint8_t *u, uint8_t *v,
                             int w, int h, int yw, int cw, int dw);

static AVFormatContext *
open_file(const char *filename)
{
    AVFormatContext *afc;
    int err = av_open_input_file(&afc, filename, NULL, 0, NULL);

    if (!err)
        err = av_find_stream_info(afc);

    if (err < 0) {
        fprintf(stderr, "%s: lavf error %d\n", filename, err);
        exit(1);
    }

    dump_format(afc, 0, filename, 0);

    return afc;
}

static AVStream *
find_stream(AVFormatContext *afc)
{
    AVStream *st = NULL;
    int i;

    for (i = 0; i < afc->nb_streams; i++) {
        if (afc->streams[i]->codec->codec_type == CODEC_TYPE_VIDEO && !st)
            st = afc->streams[i];
        else
            afc->streams[i]->discard = AVDISCARD_ALL;
    }

    return st;
}

static struct fb_var_screeninfo sinfo_p0;
static struct fb_var_screeninfo sinfo;
static struct omapfb_mem_info minfo;
static struct omapfb_plane_info pinfo;

static struct {
    unsigned x;
    unsigned y;
    uint8_t *buf;
} fb_pages[2];

static int dev_fd;
static int fb_page_flip;

static int
xioctl(const char *name, int fd, int req, void *param)
{
    int err = ioctl(fd, req, param);

    if (err == -1) {
        perror(name);
        exit(1);
    }

    return err;
}

#define xioctl(fd, req, param) xioctl(#req, fd, req, param)

static int
setup_fb(unsigned width, unsigned height, int fullscreen, int dbl_buffer)
{
    int fb = open("/dev/fb0", O_RDWR);
    uint8_t *fbmem;
    int i;

    if (fb == -1) {
        perror("/dev/fb0");
        exit(1);
    }

    xioctl(fb, FBIOGET_VSCREENINFO, &sinfo_p0);
    close(fb);

    fb = open("/dev/fb1", O_RDWR);

    if (fb == -1) {
        perror("/dev/fb1");
        exit(1);
    }

    xioctl(fb, FBIOGET_VSCREENINFO, &sinfo);
    xioctl(fb, OMAPFB_QUERY_PLANE, &pinfo);
    xioctl(fb, OMAPFB_QUERY_MEM, &minfo);

    fbmem = mmap(NULL, minfo.size, PROT_READ|PROT_WRITE, MAP_SHARED, fb, 0);
    if (fbmem == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    for (i = 0; i < minfo.size / 4; i++)
        ((uint32_t*)fbmem)[i] = 0x80008000;

    sinfo.xres = FFMIN(sinfo_p0.xres, width)  & ~15;
    sinfo.yres = FFMIN(sinfo_p0.yres, height) & ~15;
    sinfo.xoffset = 0;
    sinfo.yoffset = 0;
    sinfo.nonstd = OMAPFB_COLOR_YUY422;

    fb_pages[0].x = 0;
    fb_pages[0].y = 0;
    fb_pages[0].buf = fbmem;

    if (dbl_buffer && minfo.size >= sinfo.xres * sinfo.yres * 2) {
        sinfo.xres_virtual = sinfo.xres;
        sinfo.yres_virtual = sinfo.yres * 2;
        fb_pages[1].x = 0;
        fb_pages[1].y = sinfo.yres;
        fb_pages[1].buf = fbmem + sinfo.xres * sinfo.yres * 2;
        fb_page_flip = 1;
    }

    xioctl(fb, FBIOPUT_VSCREENINFO, &sinfo);

    pinfo.enabled = 1;
    if (fullscreen) {
        pinfo.pos_x = 0;
        pinfo.pos_y = 0;
        pinfo.out_width  = sinfo_p0.xres;
        pinfo.out_height = sinfo_p0.yres;
    } else {
        pinfo.pos_x = sinfo_p0.xres / 2 - sinfo.xres / 2;
        pinfo.pos_y = sinfo_p0.yres / 2 - sinfo.yres / 2;
        pinfo.out_width  = sinfo.xres;
        pinfo.out_height = sinfo.yres;
    }

    ioctl(fb, OMAPFB_SETUP_PLANE, &pinfo);

    return fb;
}

static int stop;

static int
ts_diff(struct timespec *tv1, struct timespec *tv2)
{
    return (tv1->tv_sec - tv2->tv_sec) * 1000 +
        (tv1->tv_nsec - tv2->tv_nsec) / 1000000;
}

static void
ts_add(struct timespec *ts, unsigned long nsec)
{
    ts->tv_nsec += nsec;
    if (ts->tv_nsec >= 1000000000) {
        ts->tv_sec++;
        ts->tv_nsec -= 1000000000;
    }
}

static struct frame {
    uint8_t *data[3];
    int pic_num;
    int next;
    int prev;
    int refs;
} *frames;

static uint8_t *frame_buf;
static unsigned num_frames;
static unsigned frame_size;
static unsigned linesize;
static int free_head;
static int free_tail;
static int disp_head = -1;
static int disp_tail = -1;
static int disp_count;

static pthread_mutex_t disp_lock;
static sem_t disp_sem;
static sem_t free_sem;

static int pic_num;

#define ALIGN(n, a) (((n)+((a)-1))&~((a)-1))

#define EDGE_WIDTH 16

static int
ofb_get_buffer(AVCodecContext *ctx, AVFrame *pic)
{
    struct frame *f = frames + free_tail;
    int i;

    sem_wait(&free_sem);

    if (free_tail < 0) {
        fprintf(stderr, "no more buffers\n");
        return -1;
    }

    for (i = 0; i < 3; i++) {
        pic->data[i] = pic->base[i] = f->data[i];
        pic->linesize[i] = linesize;
    }

    pic->type = FF_BUFFER_TYPE_USER;
    pic->age = ++pic_num - f->pic_num;
    f->pic_num = pic_num;
    f->refs++;

    free_tail = f->next;
    frames[free_tail].prev = -1;
    f->next = -1;

    return 0;
}

static void
ofb_release_frame(struct frame *f)
{
    unsigned fnum = f - frames;

    if (!--f->refs) {
        f->prev = free_head;
        if (free_head != -1)
            frames[free_head].next = fnum;
        free_head = fnum;
        sem_post(&free_sem);
    }
}

static void
ofb_release_buffer(AVCodecContext *ctx, AVFrame *pic)
{
    unsigned fnum = (pic->data[0] - frame_buf) / frame_size;
    struct frame *f = frames + fnum;
    int i;

    for (i = 0; i < 3; i++)
        pic->data[i] = NULL;

    ofb_release_frame(f);
}

static int
ofb_reget_buffer(AVCodecContext *ctx, AVFrame *pic)
{
    unsigned fnum = (pic->data[0] - frame_buf) / frame_size;
    fprintf(stderr, "reget_buffer   %2d\n", fnum);

    if (!pic->data[0]) {
        pic->buffer_hints |= FF_BUFFER_HINTS_READABLE;
        return ofb_get_buffer(ctx, pic);
    }

    return 0;
}

static void *
disp_thread(void *p)
{
    AVStream *st = p;
    unsigned long fper =
        1000000000ull * st->r_frame_rate.den / st->r_frame_rate.num;
    struct timespec ftime;
    struct timespec tstart, t1, t2;
    int nf1 = 0, nf2 = 0;
    int page = 0;
    sem_t sleep_sem;
    int sval;

    sem_init(&sleep_sem, 0, 0);

    while (sem_getvalue(&free_sem, &sval), sval && !stop)
        usleep(100000);

    clock_gettime(CLOCK_REALTIME, &tstart);
    ftime = t1 = tstart;

    while (!sem_wait(&disp_sem) && !stop) {
        struct frame *f;

        sem_timedwait(&sleep_sem, &ftime);

        pthread_mutex_lock(&disp_lock);
        f = frames + disp_tail;
        disp_tail = f->next;
        if (disp_tail != -1)
            frames[disp_tail].prev = -1;
        disp_count--;
        pthread_mutex_unlock(&disp_lock);

        f->next = -1;

        yuv420_to_yuv422(fb_pages[page].buf,
                         f->data[0], f->data[1], f->data[2],
                         sinfo.xres, sinfo.yres,
                         linesize, linesize,
                         2*sinfo.xres_virtual);

        if (fb_page_flip) {
            sinfo.xoffset = fb_pages[page].x;
            sinfo.yoffset = fb_pages[page].y;
            xioctl(dev_fd, FBIOPAN_DISPLAY, &sinfo);
            page ^= fb_page_flip;
        }

        ofb_release_frame(f);

        if (++nf1 - nf2 == 50) {
            clock_gettime(CLOCK_REALTIME, &t2);
            fprintf(stderr, "%3d fps, buffer %3d\r",
                    (nf1-nf2)*1000 / ts_diff(&t2, &t1),
                    disp_count);
            nf2 = nf1;
            t1 = t2;
        }

        ts_add(&ftime, fper);

        clock_gettime(CLOCK_REALTIME, &t2);
        if (t2.tv_sec > ftime.tv_sec ||
            (t2.tv_sec == ftime.tv_sec && t2.tv_nsec > ftime.tv_nsec))
            ftime = t2;
    }

    if (nf1) {
        clock_gettime(CLOCK_REALTIME, &t2);
        fprintf(stderr, "%3d fps\n", nf1*1000 / ts_diff(&t2, &tstart));
    }

    while (disp_tail != -1) {
        struct frame *f = frames + disp_tail;
        disp_tail = f->next;
        ofb_release_frame(f);
    }

    return NULL;
}

static void
post_frame(AVFrame *pic)
{
    unsigned fnum = (pic->data[0] - frame_buf) / frame_size;
    struct frame *f = frames + fnum;

    f->prev = disp_head;
    f->next = -1;

    if (disp_head != -1)
        frames[disp_head].next = fnum;
    disp_head = fnum;

    pthread_mutex_lock(&disp_lock);
    if (disp_tail == -1)
        disp_tail = fnum;
    disp_count++;
    pthread_mutex_unlock(&disp_lock);

    f->refs++;

    sem_post(&disp_sem);
}

static int
alloc_buffers(AVStream *st, unsigned bufsize)
{
    int buf_w, buf_h;
    unsigned frame_offset = 0;
    void *fbp;
    int i;

    buf_w = ALIGN(st->codec->width,  16);
    buf_h = ALIGN(st->codec->height, 16);

    if (!(st->codec->flags & CODEC_FLAG_EMU_EDGE)) {
        buf_w += EDGE_WIDTH * 2;
        buf_h += EDGE_WIDTH * 2;
        frame_offset = buf_w * EDGE_WIDTH + EDGE_WIDTH;
    }

    frame_size = buf_w * buf_h * 3 / 2;
    num_frames = bufsize / frame_size;
    bufsize = num_frames * frame_size;
    linesize = buf_w;

    fprintf(stderr, "Using %d frame buffers, frame_size=%d\n",
            num_frames, frame_size);

    if (posix_memalign(&fbp, 16, bufsize)) {
        fprintf(stderr, "Error allocating frame buffers: %d bytes\n", bufsize);
        exit(1);
    }

    frame_buf = fbp;
    frames = malloc(num_frames * sizeof(*frames));

    for (i = 0; i < num_frames; i++) {
        uint8_t *p = frame_buf + i * frame_size;

        frames[i].data[0] = p + frame_offset;
        frames[i].data[1] = p + buf_w * buf_h + frame_offset / 2;
        frames[i].data[2] = frames[i].data[1] + buf_w / 2;

        frames[i].pic_num = -num_frames;
        frames[i].next = i + 1;
        frames[i].prev = i - 1;
        frames[i].refs = 0;
    }

    free_head = num_frames - 1;
    frames[free_head].next = -1;

    return 0;
}

static void
sigint(int s)
{
    stop = 1;
    sem_post(&disp_sem);
}

static int
speed_test(char *size, int fullscreen, int dbl_buffer)
{
    struct timespec t1, t2;
    uint8_t *y, *u, *v;
    unsigned w, h = 0;
    unsigned n = 1000;
    unsigned bufsize;
    int page = 0;
    void *buf;
    int i, j;
    int fd;

    w = strtoul(size, &size, 0);
    if (*size++)
        h = strtoul(size, &size, 0);
    if (*size++)
        n = strtoul(size, NULL, 0);

    w &= ~15;
    h &= ~15;

    if (!w || !h || !n) {
        fprintf(stderr, "Invalid size/count '%s'\n", size);
        return 1;
    }

    bufsize = w * h * 3 / 2;
    if (posix_memalign(&buf, 16, bufsize)) {
        fprintf(stderr, "Error allocating %u bytes\n", bufsize);
        return 1;
    }

    y = buf;
    u = y + w * h;
    v = u + w / 2;

    memset(y, 128, w * h);

    for (i = 0; i < h / 2; i++) {
        for (j = 0; j < w / 2; j++) {
            u[i*w + j] = 2*i;
            v[i*w + j] = 2*j;
        }
    }

    fd = setup_fb(w, h, fullscreen, dbl_buffer);
    signal(SIGINT, sigint);

    clock_gettime(CLOCK_REALTIME, &t1);

    for (i = 0; i < n && !stop; i++) {
        yuv420_to_yuv422(fb_pages[page].buf,
                         y, u, v, w, h, w, w,
                         2*sinfo.xres_virtual);

        if (fb_page_flip) {
            sinfo.xoffset = fb_pages[page].x;
            sinfo.yoffset = fb_pages[page].y;
            xioctl(fd, FBIOPAN_DISPLAY, &sinfo);
            page ^= fb_page_flip;
        }
    }

    clock_gettime(CLOCK_REALTIME, &t2);
    j = ts_diff(&t2, &t1);
    fprintf(stderr, "%d ms, %d fps, read %lld B/s, write %lld B/s\n",
            j, i*1000 / j, 1000LL*i*bufsize / j, 2000LL*i*w*h / j);

    pinfo.enabled = 0;
    ioctl(fd, OMAPFB_SETUP_PLANE, &pinfo);
    close(fd);

    free(buf);

    return 0;
}

int
main(int argc, char **argv)
{
    AVFormatContext *afc;
    AVCodec *codec;
    AVCodecContext *avc;
    AVStream *st;
    AVPacket pk;
    int bufsize = BUFFER_SIZE;
    pthread_t dispt;
    int fullscreen = 0;
    int dbl_buffer = 1;
    char *test_param = NULL;
    int opt;
    int err;

    while ((opt = getopt(argc, argv, "b:fst:")) != -1) {
        switch (opt) {
        case 'b':
            bufsize = strtol(optarg, NULL, 0) * 1048576;
            break;
        case 'f':
            fullscreen = 1;
            break;
        case 's':
            dbl_buffer = 0;
            break;
        case 't':
            test_param = optarg;
            break;
        }
    }

    argc -= optind;
    argv += optind;

    if (test_param)
        return speed_test(test_param, fullscreen, dbl_buffer);

    if (argc < 1)
        return 1;

    av_register_all();
    avcodec_register_all();

    afc = open_file(argv[0]);

    st = find_stream(afc);
    if (!st) {
        fprintf(stderr, "No video streams found.\n");
        exit(1);
    }

    codec = avcodec_find_decoder(st->codec->codec_id);
    if (!codec) {
        fprintf(stderr, "Can't find codec %x\n", st->codec->codec_id);
        exit(1);
    }

    alloc_buffers(st, bufsize);

    pthread_mutex_init(&disp_lock, NULL);
    sem_init(&disp_sem, 0, 0);
    sem_init(&free_sem, 0, num_frames - 1);

    avc = avcodec_alloc_context();

    avc->width          = st->codec->width;
    avc->height         = st->codec->height;
    avc->time_base      = st->codec->time_base;
    avc->extradata      = st->codec->extradata;
    avc->extradata_size = st->codec->extradata_size;

    avc->get_buffer     = ofb_get_buffer;
    avc->release_buffer = ofb_release_buffer;
    avc->reget_buffer   = ofb_reget_buffer;

    err = avcodec_open(avc, codec);
    if (err) {
        fprintf(stderr, "avcodec_open: %d\n", err);
        exit(1);
    }

    dev_fd = setup_fb(st->codec->width, st->codec->height,
                      fullscreen, dbl_buffer);

    signal(SIGINT, sigint);

    pthread_create(&dispt, NULL, disp_thread, st);

    while (!stop && !av_read_frame(afc, &pk)) {
        AVFrame f;
        int gp = 0;

        if (pk.stream_index == st->index) {
            avcodec_decode_video(avc, &f, &gp, pk.data, pk.size);

            if (gp) {
                post_frame(&f);
            }
        }

        av_free_packet(&pk);
    }

    if (!stop) {
        while (disp_tail != -1)
            usleep(100000);
    }

    stop = 1;
    sem_post(&disp_sem);
    pthread_join(dispt, NULL);

    pinfo.enabled = 0;
    ioctl(dev_fd, OMAPFB_SETUP_PLANE, &pinfo);
    close(dev_fd);

    avcodec_close(avc);
    av_close_input_file(afc);

    free(frame_buf);
    free(frames);

    return 0;
}
