#include <myegl.h>

#include <GLES2/gl2.h>

#include <stdio.h>
#include <stdlib.h>

const EGLint attrib_required[] = {
	EGL_COLOR_BUFFER_TYPE, EGL_RGB_BUFFER,
	EGL_RED_SIZE, 8,
	EGL_GREEN_SIZE, 8,
	EGL_BLUE_SIZE, 8,
	EGL_ALPHA_SIZE, 8,
	EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
	EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
	EGL_NATIVE_RENDERABLE, EGL_TRUE,
	EGL_NATIVE_VISUAL_ID, GBM_FORMAT_XRGB8888,
//	EGL_NATIVE_VISUAL_TYPE, ? 
EGL_NONE};

struct egl_info_t *create_gles_context(struct info2_t *info2) {
	EGLint major, minor;

	struct egl_info_t *E = malloc(sizeof(*E));

	E->dpy = eglGetPlatformDisplay(EGL_PLATFORM_GBM_MESA, info2->gbm, NULL);
	E->ctx = EGL_NO_CONTEXT;
	if (E->dpy == EGL_NO_DISPLAY) {
		fprintf(stderr, "eglGetPlatformDisplay failed\n");
	}
	printf("\nEGL\neglGetPlatformDisplay successful\n");

	if (eglInitialize(E->dpy, &major, &minor) == EGL_FALSE) {
		fprintf(stderr, "eglInitialize failed\n");
	}
	printf("eglInitialize successful (EGL %i.%i)\n", major, minor);
	
	if (eglBindAPI(EGL_OPENGL_ES_API == EGL_FALSE)) {
		fprintf(stderr, "eglBindAPI failed\n");
	}
	printf("eglBindAPI successful\n");

	// `size` is the upper value of the possible values of `matching`
	const int size = 1;
	int matching;
	EGLConfig *config = malloc(size*sizeof(EGLConfig));
	eglChooseConfig(E->dpy, attrib_required, config, size, &matching);
	printf("EGLConfig matching: %i (requested: %i)\n", matching, size);
	
	const EGLint attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
	
	E->ctx = eglCreateContext(E->dpy, *config, EGL_NO_CONTEXT, attribs);
	if (E->ctx == EGL_NO_CONTEXT) {
		fprintf(stderr, "eglGetCreateContext failed\n");
	}
	printf("eglCreateContext successful\n");
	E->surf = eglCreatePlatformWindowSurface(E->dpy, *config, info2->surf,
	NULL);
	if (E->surf == EGL_NO_SURFACE) {
		fprintf(stderr, "eglCreatePlatformWindowSurface failed\n");
	}
	printf("eglCreatePlatformWindowSurface successful\n");

	if (eglMakeCurrent(E->dpy, E->surf, E->surf, E->ctx) == EGL_FALSE) {
		fprintf(stderr, "eglMakeCurrent failed\n");
	}
	printf("eglMakeCurrent successful\n");

	glClearColor(0, 0, 1, 1);
	glClear(GL_COLOR_BUFFER_BIT);

	if (eglSwapBuffers(E->dpy, E->surf) == EGL_FALSE) {
		fprintf(stderr, "eglSwapBuffers failed\n");
	}
	printf("eglSwapBuffers successful\n");
		
	return 0;
}

int destroy_gles_context(struct egl_info_t *E, struct info2_t *info2) {
	if (eglMakeCurrent(E->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) == EGL_FALSE) {
		fprintf(stderr, "eglMakeCurrent failed\n");
	}
	printf("eglMakeCurrent successful\n");
	
	gbm_surface_release_buffer(info2->surf, info2->bo);
	printf("surf: %i\n", eglDestroySurface(E->dpy, E->surf));
	gbm_surface_destroy(info2->surf);

	if (eglMakeCurrent(EGL_NO_CONTEXT, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) == EGL_FALSE) {
		fprintf(stderr, "eglMakeCurrent failed\n");
	}
	printf("eglMakeCurrent successful\n");

	printf("ctx: %i\n", eglDestroyContext(E->dpy, E->ctx));
	printf("dpy: %i\n", eglTerminate(E->dpy));
	return 0;
}
