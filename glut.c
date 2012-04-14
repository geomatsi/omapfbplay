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
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <semaphore.h>

#include <GL/glut.h>
#include <GL/glu.h>
#include <GL/gl.h>

#include "display.h"
#include "pixfmt.h"
#include "memman.h"
#include "util.h"

static const struct pixfmt *dfmt;
static enum PixelFormat img_fmt;
static GLubyte *img_ptr;
static GLuint img_h;
static GLuint img_w;

static GLuint texName;

static GLuint rotation = 0;
static GLfloat spin = 0.0;

static pthread_t glt;
static sem_t glut_sem;

volatile int tmp = 0;
int tflag = 0;

static void updateFrame(struct frame *f)
{
    int i, j, k;
    int hsub, vsub;

    uint8_t r, g, b, t;
    uint8_t *yp, *up, *vp;
    uint8_t y, u, v;


    hsub = dfmt->hsub[1];
    vsub = dfmt->vsub[1];

    yp = f->vdata[dfmt->plane[0]] + dfmt->start[0];
    up = f->vdata[dfmt->plane[1]] + dfmt->start[1];
    vp = f->vdata[dfmt->plane[2]] + dfmt->start[2];

#if 0
    for (i = 0; i < img_h; i++) {
        for (j = 0; j < img_w; j++) {
            r = yp[i*f->linesize[dfmt->plane[0]] + j*dfmt->inc[0]];

            *(img_ptr + 4*i*img_w + 4*j + 0) = (GLubyte) r;
            *(img_ptr + 4*i*img_w + 4*j + 1) = (GLubyte) r;
            *(img_ptr + 4*i*img_w + 4*j + 2) = (GLubyte) r;
            *(img_ptr + 4*i*img_w + 4*j + 3) = (GLubyte) 128;
        }
    }
#else
    k = 0;

    for (i = 0; i < img_h; i++) {
        for (j = 0; j < img_w; j++, k += 4) {
            y = yp[i*f->linesize[dfmt->plane[0]] + j*dfmt->inc[0]];
            u = up[(i*f->linesize[dfmt->plane[1]] + j*dfmt->inc[1]) >> vsub];
            v = vp[(i*f->linesize[dfmt->plane[2]] + j*dfmt->inc[2]) >> hsub];

            *(img_ptr + k + 0) = y + 1.402*(v-128);
            *(img_ptr + k + 1) = y - 0.34414*(u-128) - 0.71414*(v-128);
            *(img_ptr + k + 2) = y + 1.772*(u-128);
            *(img_ptr + k + 3) = 128;
        }
    }
#endif
}

static void updateTexture()
{
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glDeleteTextures(1, &texName);
    glGenTextures(1, &texName);
    glBindTexture(GL_TEXTURE_2D, texName);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img_w, img_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, img_ptr);
}

void display(void)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    gluPerspective(60, 1, 1, 10); /* fov, aspect, near, far */
    gluLookAt(0, 0, 6, /* */ 0, 0, 0, /* */ 0, 1, 0); /* eye, center, up */
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_NORMALIZE);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glPushAttrib(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

#if 0

    glRotatef(spin, 0.0, 0.0, 1.0);

    glBegin(GL_QUADS);
    glNormal3f(0.0, 0.0, -1.0);
    glTexCoord2d(0, 1); glVertex3f(-2.0, -2.0, 1.0);
    glTexCoord2d(0, 0); glVertex3f(-2.0, 2.0, 1.0);
    glTexCoord2d(1, 0); glVertex3f(2.0, 2.0, -1.0);
    glTexCoord2d(1, 1); glVertex3f(2.0, -2.0, -1.0);
    glEnd();

#else

    glRotatef(spin, 0.0, 1.0, 1.0);

    glBegin(GL_QUADS);
    glNormal3f(0.0, 0.0, 1.0);
    glTexCoord2d(1, 1); glVertex3f(-1.0, -1.0, -1.0);
    glTexCoord2d(1, 0); glVertex3f(-1.0, 1.0, -1.0);
    glTexCoord2d(0, 0); glVertex3f(1.0, 1.0, -1.0);
    glTexCoord2d(0, 1); glVertex3f(1.0, -1.0, -1.0);
    glEnd();

    glBegin(GL_QUADS);
    glNormal3f(0.0, 1.0, 0.0);
    glTexCoord2d(1, 1); glVertex3f(-1.0, -1.0, 1.0);
    glTexCoord2d(1, 0); glVertex3f(1.0, -1.0, 1.0);
    glTexCoord2d(0, 0); glVertex3f(1.0, -1.0, -1.0);
    glTexCoord2d(0, 1); glVertex3f(-1.0, -1.0, -1.0);
    glEnd();

    glBegin(GL_QUADS);
    glNormal3f(0.0, -1.0, 0.0);
    glTexCoord2d(1, 1); glVertex3f(-1.0, 1.0, 1.0);
    glTexCoord2d(1, 0); glVertex3f(1.0, 1.0, 1.0);
    glTexCoord2d(0, 0); glVertex3f(1.0, 1.0, -1.0);
    glTexCoord2d(0, 1); glVertex3f(-1.0, 1.0, -1.0);
    glEnd();

    glBegin(GL_QUADS);
    glNormal3f(0.0, 0.0, -1.0);
    glTexCoord2d(1, 1); glVertex3f(-1.0, -1.0, 1.0);
    glTexCoord2d(1, 0); glVertex3f(-1.0, 1.0, 1.0);
    glTexCoord2d(0, 0); glVertex3f(1.0, 1.0, 1.0);
    glTexCoord2d(0, 1); glVertex3f(1.0, -1.0, 1.0);
    glEnd();

    glBegin(GL_QUADS);
    glNormal3f(1.0, 0.0, 0.0);
    glTexCoord2d(1, 1); glVertex3f(-1.0, -1.0, 1.0);
    glTexCoord2d(1, 0); glVertex3f(-1.0, 1.0, 1.0);
    glTexCoord2d(0, 0); glVertex3f(-1.0, 1.0, -1.0);
    glTexCoord2d(0, 1); glVertex3f(-1.0, -1.0, -1.0);
    glEnd();

    glBegin(GL_QUADS);
    glNormal3f(-1.0, 0.0, 0.0);
    glTexCoord2d(1, 1); glVertex3f(1.0, -1.0, 1.0);
    glTexCoord2d(1, 0); glVertex3f(1.0, 1.0, 1.0);
    glTexCoord2d(0, 0); glVertex3f(1.0, 1.0, -1.0);
    glTexCoord2d(0, 1); glVertex3f(1.0, -1.0, -1.0);
    glEnd();

#endif

    glPopAttrib();
    glFlush();
    glutSwapBuffers();
}

void updateDisplay(void)
{
    if (rotation % 2) {
        spin += 2.0;

	    if (spin > 360.0)
		    spin -= 360.0;
    }

    if (0 == sem_trywait(&glut_sem)) {
        updateTexture();
        sem_post(&glut_sem);
    }

    glutPostRedisplay();
}

void keyboard (unsigned char key, int x, int y)
{
    switch (key) {
        case 'q':       /* quit */
            printf("exit...\n");
            exit(0);
            break;
        case 's':       /* rotation */
            rotation += 1;
            break;
        default:
            break;
    }
}

void reshape(int w, int h)
{
	glViewport(0, 0, (GLsizei) w, (GLsizei) h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
    glOrtho(-100.0, 100.0, -100.0, 100.0, -100.0, 100.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

static void * glut_thread(void *p)
{
    glutMainLoop();
    return NULL;
}

static int glut_open(const char *name, struct frame_format *dp,
        struct frame_format *ff)
{
    char **argv = NULL;
    int argc = 0;

    GLfloat mat_specular[] = { 1.0, 1.0, 1.0, 1.0 };
	GLfloat mat_shininess[] = { 50.0 };
	GLfloat light_position[] = { 0.0, 0.0, -5.0, 1.0 };
	GLfloat white_light[] = { 1.0, 1.0, 1.0, 1.0 };
	GLfloat lmodel_ambient[] = { 0.1, 0.1, 0.1, 1.0 };

    /* GLUT init */
    glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	glutInitWindowSize(640, 480);
	glutInitWindowPosition(50, 50);
	glutCreateWindow("glut");

	glClearColor(0.0, 0.0, 0.0, 0.0);
	glShadeModel(GL_SMOOTH);

	glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
	glMaterialfv(GL_FRONT, GL_SHININESS, mat_shininess);

	glLightfv(GL_LIGHT0, GL_POSITION, light_position);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, white_light);
	glLightfv(GL_LIGHT0, GL_SPECULAR, white_light);

	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lmodel_ambient);

	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);

    glutDisplayFunc(display);
	glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutIdleFunc(updateDisplay);

    /* display settings */

    dp->width  = 640;
    dp->height = 480;
    dp->pixfmt = PIX_FMT_YUYV422;
    dp->y_stride  = 2 * ALIGN(ff->disp_w, 16);
    dp->uv_stride = 0;

    /* misc init */

    srandom((unsigned int) getpid());
    sem_init(&glut_sem, 0, 1);

    /* allocate memory for image */
    do {
        int bufsize;

        img_fmt = ff->pixfmt;
        dfmt = ofbp_get_pixfmt(ff->pixfmt);

        if (!dfmt) {
            fprintf(stderr, "Unknown pixel format %d\n", ff->pixfmt);
            return -1;
        }

        img_h = ff->height;
        img_w = ff->width;

        bufsize = img_h * img_w * 4;
        if (posix_memalign((void **) &img_ptr, 32, bufsize)) {
            fprintf(stderr, "Error allocating frame buffers: %d bytes\n", bufsize);
            return -1;
        }

    } while (0);

    return 0;
}

static int glut_enable(struct frame_format *ff, unsigned flags,
        const struct pixconv *pc, struct frame_format *df)
{
    pthread_create(&glt, NULL, glut_thread, NULL);
    return 0;
}

static void glut_prepare(struct frame *f)
{
    sem_wait(&glut_sem);
    updateFrame(f);
    sem_post(&glut_sem);
}

static void glut_show(struct frame *f)
{
    ofbp_put_frame(f);
}

static void glut_close(void)
{

}

DISPLAY(glut) = {
    .name  = "glut",
    .flags = OFBP_DOUBLE_BUF | OFBP_PRIV_MEM,
    .open  = glut_open,
    .enable  = glut_enable,
    .prepare = glut_prepare,
    .show  = glut_show,
    .close = glut_close,
};
