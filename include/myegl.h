#ifndef MYEGL_H
#define MYEGL_H

#include <gbm.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

struct egl_info_t {
	EGLDisplay dpy;
	EGLContext ctx;
};

struct egl_info_t *create_gles_context(struct gbm_device *gbm_dev);
int destroy_gles_context(struct egl_info_t *egl_info);

#endif
