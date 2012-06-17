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
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <semaphore.h>

#include <GLES/egl.h>
#include <GLES/gl.h>

#include "display.h"
#include "pixfmt.h"
#include "memman.h"
#include "util.h"

/* */

static EGLDisplay  eglDisplay	= 0;
static EGLConfig   eglConfig	= 0;
static EGLSurface  eglSurface	= 0;
static EGLContext  eglContext	= 0;

static GLuint *img_ptr;
static GLuint img_h;
static GLuint img_w;

static GLuint tex;
static GLuint vbo;

static GLfloat spin = 1.0;

static const struct pixconv *pixconv;
static sem_t gles_sem;
static pthread_t glt;

/* */

static inline uint32_t UP_PWR2(uint32_t x)
{
        return 1 << (32 - __builtin_clz (x - 1));
}

static inline uint32_t DOWN_PWR2(uint32_t x)
{
        return 1 << ((32 - __builtin_clz (x - 1)) - 1);
}

static int TestEGLError(char* msg)
{
    EGLint err = eglGetError();
    if (err != EGL_SUCCESS)
    {
        fprintf(stderr, "EGL: %s failed (%d).\n", msg, err);
        return 0;
    }

    return 1;
}

static void TestGLError(char* msg)
{
    GLenum err = glGetError();

    if (err != GL_NO_ERROR) {
        fprintf(stderr, "OPENGL: %s failed with (%d).\n", msg, err);
    }

    return;
}

static inline void convert_frame_test(GLubyte val)
{
    int i, j;

    for(i = 0; i < img_h; ++i) {
        for(j = 0; j < img_w; ++j)
        {
            GLuint col = (val<<24) + (val<<16) + (val<<8) + (val<<0);
            img_ptr[j*img_h + i] = col;
        }
    }
}

static void update_texture()
{
    glDisable(GL_TEXTURE_2D);
    glDeleteTextures(1, &tex);
    glEnable(GL_TEXTURE_2D);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img_w, img_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, img_ptr);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

static void display(void)
{
    glRotatef(spin, 0.0, 1.0, 1.0);
    TestGLError("glRotate");

    glClear(GL_COLOR_BUFFER_BIT);
    TestGLError("glClear");

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    TestGLError("glBindBuffer");

    glEnableClientState(GL_VERTEX_ARRAY);
    TestGLError("glEnableClientState");

    glVertexPointer(3, GL_FLOAT, sizeof(GLfloat) * 5, 0);
    TestGLError("glVertexPointer");

    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    TestGLError("glEnableClientState");

	glTexCoordPointer(2, GL_FLOAT, sizeof(GLfloat) * 5, (unsigned char*) (sizeof(GLfloat) * 3));
    TestGLError("glTexCoordPointer");

    glDrawArrays(GL_TRIANGLES, 0, 3);
    TestGLError("glDrawArrays");

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    TestGLError("glBindBuffer");

    eglSwapBuffers(eglDisplay, eglSurface);
    TestEGLError("eglSwapBuffers");
}

static int gles_open(const char *name, struct frame_format *dp,
        struct frame_format *ff)
{
    /* EGL vars */

    EGLint iMajorVersion, iMinorVersion;
	int iConfigs;

    EGLint pi32ConfigAttribs[3];

    pi32ConfigAttribs[0] = EGL_SURFACE_TYPE;
    pi32ConfigAttribs[1] = EGL_WINDOW_BIT;
    pi32ConfigAttribs[2] = EGL_NONE;

    /* EGL init */

	/* Step 1 - Get the default display */

    eglDisplay = eglGetDisplay((NativeDisplayType) 0);

	/* Step 2 - Initialize EGL */

	if (!eglInitialize(eglDisplay, &iMajorVersion, &iMinorVersion)) {
		fprintf(stderr, "Error: eglInitialize() failed.\n");
		goto cleanup;
	}

	/* Step 3 - Find a config that matches all requirements */

	if (!eglChooseConfig(eglDisplay, pi32ConfigAttribs, &eglConfig, 1, &iConfigs) || (iConfigs != 1)) {
		fprintf(stderr, "Error: eglChooseConfig() failed.\n");
		goto cleanup;
	}

    /* Step 4 - Create a surface to draw to */

	eglSurface = eglCreateWindowSurface(eglDisplay, eglConfig, (NativeWindowType) NULL, NULL);

	if (!TestEGLError("eglCreateWindowSurface")) {
		goto cleanup;
	}

	/* Step 5 - Create a context */

	eglContext = eglCreateContext(eglDisplay, eglConfig, NULL, NULL);

	if (!TestEGLError("eglCreateContext")) {
		goto cleanup;
	}

    /* display settings */

    dp->pixfmt = PIX_FMT_RGBA;

    /* misc init */

    sem_init(&gles_sem, 0, 1);

    /* allocate memory for image */

    do {
        int bufsize;

        img_h = DOWN_PWR2(ff->height);
        img_w = DOWN_PWR2(ff->width);

        bufsize = img_h * img_w * 4;

        if (posix_memalign((void **) &img_ptr, 4, bufsize)) {
            fprintf(stderr, "Error allocating frame buffers: %d bytes\n", bufsize);
            goto cleanup;
        }

    } while (0);

    /* debug print */
    printf(">>> dump codec frame info:\n");
    printf(">>> (width, height) = (%d, %d)\n", ff->width, ff->height);
    printf(">>> (disp_x, disp_y) = (%d, %d)\n", ff->disp_x, ff->disp_y);
    printf(">>> (disp_w, disp_h) = (%d, %d)\n", ff->disp_w, ff->disp_h);
    printf(">>> (y_stride, uv_stride) = (%d, %d)\n", ff->y_stride, ff->uv_stride);
    printf(">>> (img_h, img_w) = (%d, %d)\n", img_h, img_w);

    return 0;

cleanup:

	eglTerminate(eglDisplay);
    return -1;
}

static void * gles_thread(void *p)
{

    GLfloat afVertices[] = {
        -0.4f, -0.4f, 0.0f,
        +0.0f, +0.0f,
        +0.4f, -0.4f, 0.0f,
        +1.0f, +0.0f,
        +0.4f, +0.4f, 0.0f,
        +0.5f, +1.0f,
    };

    unsigned int sz = 3 * (sizeof(GLfloat) * 5);

    /* we make current context in this thread */

    eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext);

    if (!TestEGLError("eglMakeCurrent")) {
        goto display_out;
    }


    /* create VBO for triangles */

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sz, afVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);


	if (!TestEGLError("eglMakeCurrent")) {
		goto display_out;
	}

    while (1) {

        sem_wait(&gles_sem);
        update_texture();
        sem_post(&gles_sem);
        display();
        //fprintf(stderr, "INFO: frame displayed\n");
    }

    glDeleteBuffers(1, &vbo);
	glDeleteTextures(1, &tex);

display_out:

    /* delete context */
	eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglTerminate(eglDisplay);

    return NULL;
}


static int gles_enable(struct frame_format *ff, unsigned flags,
        const struct pixconv *pc, struct frame_format *df)
{
    pixconv = pc;

    pthread_create(&glt, NULL, gles_thread, NULL);
    return 0;
}

static inline void convert_frame(struct frame *f)
{
    pixconv->convert((uint8_t **)img_ptr, (uint8_t **)f, (uint8_t **) img_h, (uint8_t **)img_w);
}

static void gles_prepare(struct frame *f)
{
    sem_wait(&gles_sem);
    convert_frame(f);
}

static void gles_show(struct frame *f)
{
    pixconv->finish();
    ofbp_put_frame(f);
    sem_post(&gles_sem);
}

static void gles_close(void)
{
	eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) ;
	eglTerminate(eglDisplay);
}

DISPLAY(gles) = {
    .name  = "gles-ws",
    .flags = OFBP_DOUBLE_BUF | OFBP_PRIV_MEM,
    .open  = gles_open,
    .enable  = gles_enable,
    .prepare = gles_prepare,
    .show  = gles_show,
    .close = gles_close,
};
