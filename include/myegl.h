#ifndef MYEGL_H
#define MYEGL_H

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <gbm.h>

struct renderer_info_t {
	EGLDisplay dpy;
	EGLContext ctx;
	EGLSurface surf_egl;

	struct gbm_device *gbm;
	struct gbm_surface *surf_gbm;
	struct gbm_bo *bo;
};

#include <mydrm.h>

struct renderer_info_t *create_renderer(struct drm_info_t *drm_info, int fd);

int render();

int destroy_renderer(struct renderer_info_t *renderer_info);

#endif
