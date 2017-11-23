#include <myegl.h>

#include <stdio.h>
#include <stdlib.h>

struct egl_info_t *create_gles_context(struct gbm_device *gbm_dev) {
	EGLint major, minor;

	struct egl_info_t *E = calloc(1, sizeof(*E));
	E->dpy = EGL_NO_DISPLAY;
	E->ctx = EGL_NO_CONTEXT;

	E->dpy = eglGetPlatformDisplay(EGL_PLATFORM_GBM_MESA, gbm_dev, NULL);

	eglInitialize(E->dpy, &major, &minor);
	printf("EGL %i.%i\n", major, minor);

	eglBindAPI(EGL_OPENGL_ES_API);

//	E->ctx = eglCreateContext(E->dpy, config, EGL_NO_CONTEXT, attrib_list);
	return 0;
}

int destroy_gles_context(struct egl_info_t *E) {
	eglDestroyContext(E->dpy, E->ctx);
	eglTerminate(E->dpy);
	return 0;
}
