#ifndef MYEGL_H
#define MYEGL_H

#include <mydrm.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

struct egl_info_t {
	EGLDisplay dpy;
	EGLContext ctx;
	EGLSurface surf;
};

struct egl_info_t *create_gles_context(struct info2_t *info2);
int destroy_gles_context(struct egl_info_t *egl_info, struct info2_t *info2);

#endif
