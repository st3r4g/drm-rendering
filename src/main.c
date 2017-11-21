#include <mysession.h>
#include <mydrm.h>
#include <myegl.h>

#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include <gbm.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

int main() {
//	bool res;

/*	struct gbm_device *gbm = gbm_create_device(fd);
	assert(gbm != NULL);

	EGLDisplay egl_dpy = eglGetPlatformDisplay(EGL_PLATFORM_GBM_MESA, gbm,
	NULL);

	assert(egl_dpy != NULL);

	res = eglInitialize(egl_dpy, NULL, NULL);
	assert(res);
*/
	display_info("/dev/dri/card0");
	display_info("/dev/dri/card1");
	return 0;
}
