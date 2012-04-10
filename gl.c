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

#include <GL/glut.h>
#include <GL/glu.h>
#include <GL/gl.h>

#include "display.h"
#include "memman.h"
#include "util.h"

#define TH 64
#define TW 64

static GLubyte image[TH][TW][4];
static GLuint rotation = 0;
static GLfloat spin = 0.0;
static GLuint texName;
static pthread_t glt;

volatile int gl_flag = 0;

static void updateTexture()
{
    int i, j, r, g, b;

    for (i = 0; i < TH; i++) {
        for (j = 0; j < TW; j++) {

            r = random() % 255;
            g = random() % 255;
            b = random() % 255;

            image[i][j][0] = (GLubyte) r;
            image[i][j][1] = (GLubyte) g;
            image[i][j][2] = (GLubyte) b;
            image[i][j][3] = (GLubyte) 128;
        }
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glDeleteTextures(1, &texName);
    glGenTextures(1, &texName);
    glBindTexture(GL_TEXTURE_2D, texName);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TW, TH, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
}

void display(void)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    gluPerspective(60, 1, 1, 10); /* fov, aspect, near, far */
    gluLookAt(0, 0, 6, /* */ 0, 0, 0, /* */ 0, 1, 0); /* eye, center, up */
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glRotatef(spin, 1.0, 1.0, 1.0);
    glEnable(GL_NORMALIZE);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glPushAttrib(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_TEXTURE_2D);

    glBegin(GL_QUADS);
    glNormal3f(0.0, 0.0, -1.0);
    glTexCoord2d(1, 1); glVertex3f(-1.0, -1.0, 1.0);
    glTexCoord2d(1, 0); glVertex3f(-1.0, 1.0, 1.0);
    glTexCoord2d(0, 0); glVertex3f(1.0, 1.0, -1.0);
    glTexCoord2d(0, 1); glVertex3f(1.0, -1.0, -1.0);
    glEnd();

    glDisable(GL_TEXTURE_2D);
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

    if (gl_flag) {
        updateTexture();
        gl_flag = 0;
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
        case 'n':       /* refresh */
            printf("refresh texture...\n");
            updateTexture();
            if (!(rotation % 2)){
	            glutPostRedisplay();
            }
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

static void * gl_thread(void *p)
{
    glutMainLoop();
    return NULL;
}

static int gl_open(const char *name, struct frame_format *dp,
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
	glutCreateWindow("gl");

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

    glutDisplayFunc(display);
	glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutIdleFunc(updateDisplay);

    /* misc init */

    srandom((unsigned int) getpid());

    /* display settings */

    dp->width  = 640;
    dp->height = 480;
    dp->pixfmt = PIX_FMT_YUYV422;
    dp->y_stride  = 2 * ALIGN(ff->disp_w, 16);
    dp->uv_stride = 0;

    return 0;
}

static int gl_enable(struct frame_format *ff, unsigned flags,
        const struct pixconv *pc, struct frame_format *df)
{
    pthread_create(&glt, NULL, gl_thread, NULL);
    return 0;
}

static void gl_prepare(struct frame *f)
{
    gl_flag = 1;
}

static void gl_show(struct frame *f)
{
    ofbp_put_frame(f);
}

static void gl_close(void)
{

}

DISPLAY(gl) = {
    .name  = "gl",
    .flags = OFBP_DOUBLE_BUF | OFBP_PRIV_MEM,
    .open  = gl_open,
    .enable  = gl_enable,
    .prepare = gl_prepare,
    .show  = gl_show,
    .close = gl_close,
};
