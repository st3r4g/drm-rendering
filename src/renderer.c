#include <myrenderer.h>

#include <GLES2/gl2.h>

#include <stdio.h>
#include <stdlib.h>

const char *get_error(EGLint error_code) {
	switch (error_code) {
	case EGL_SUCCESS:             return "EGL_SUCCESS";
	case EGL_NOT_INITIALIZED:     return "EGL_NOT_INITITALIZED";
	case EGL_BAD_ACCESS:          return "EGL_BAD_ACCESS";
	case EGL_BAD_ALLOC:           return "EGL_BAD_ALLOC";
	case EGL_BAD_ATTRIBUTE:       return "EGL_BAD_ATTRIBUTE";
	case EGL_BAD_CONTEXT:         return "EGL_BAD_CONTEXT";
	case EGL_BAD_CONFIG:          return "EGL_BAD_CONFIG";
	case EGL_BAD_CURRENT_SURFACE: return "EGL_BAD_CURRENT_SURFACE";
	case EGL_BAD_DISPLAY:         return "EGL_BAD_DISPLAY";
	case EGL_BAD_SURFACE:         return "EGL_BAD_SURFACE";
	case EGL_BAD_MATCH:           return "EGL_BAD_MATCH";
	case EGL_BAD_PARAMETER:       return "EGL_BAD_PARAMETER";
	case EGL_BAD_NATIVE_PIXMAP:   return "EGL_BAD_NATIVE_PIXMAP";
	case EGL_BAD_NATIVE_WINDOW:   return "EGL_BAD_NATIVE_WINDOW";
	case EGL_CONTEXT_LOST:        return "EGL_CONTEXT_LOST";
	default:                      return "Unknown error";
	}
}

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

// `private` functions definitions
int create_memory_buffer(struct renderer_info_t *Ri, struct drm_info_t *Di, int fd); 
int create_renderer_context(struct renderer_info_t *Ri);

struct renderer_info_t *create_renderer(struct drm_info_t *Di, int fd) {
	struct renderer_info_t *Ri = malloc(sizeof(*Ri));
	create_memory_buffer(Ri, Di, fd);
	create_renderer_context(Ri);
	return Ri;
}

int create_memory_buffer(struct renderer_info_t *Ri, struct drm_info_t *Di, int fd) {
	Ri->gbm = gbm_create_device(fd);
	if (Ri->gbm == NULL) {
		fprintf(stderr, "gbm_create_device failed\n");
	}
	printf("\ngbm_create_device successful\n");

	int ret = gbm_device_is_format_supported(Ri->gbm, GBM_BO_FORMAT_XRGB8888,
	GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
	if (!ret) {
		fprintf(stderr, "format unsupported\n");
		return -1;
	}
	printf("format supported\n");

	Ri->surf_gbm= gbm_surface_create(Ri->gbm, Di->mode.hdisplay,
	Di->mode.vdisplay, GBM_BO_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT |
	GBM_BO_USE_RENDERING);
	if (Ri->surf_gbm == NULL) {
		fprintf(stderr, "gbm_surface_create failed\n");
	}
	printf("gbm_surface_create successful\n");

	return 0;
}

int create_renderer_context(struct renderer_info_t *Ri) {
	EGLint major, minor;

	Ri->dpy = eglGetPlatformDisplay(EGL_PLATFORM_GBM_MESA, Ri->gbm, NULL);
	Ri->ctx = EGL_NO_CONTEXT;
	if (Ri->dpy == EGL_NO_DISPLAY) {
		fprintf(stderr, "eglGetPlatformDisplay failed\n");
	}
	printf("\nEGL\neglGetPlatformDisplay successful\n");

	if (eglInitialize(Ri->dpy, &major, &minor) == EGL_FALSE) {
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
	eglChooseConfig(Ri->dpy, attrib_required, config, size, &matching);
	printf("EGLConfig matching: %i (requested: %i)\n", matching, size);
	
	const EGLint attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
	
	Ri->ctx = eglCreateContext(Ri->dpy, *config, EGL_NO_CONTEXT, attribs);
	if (Ri->ctx == EGL_NO_CONTEXT) {
		fprintf(stderr, "eglGetCreateContext failed\n");
	}
	printf("eglCreateContext successful\n");
	Ri->surf_egl = eglCreatePlatformWindowSurface(Ri->dpy, *config, Ri->surf_gbm, NULL);
	if (Ri->surf_egl == EGL_NO_SURFACE) {
		fprintf(stderr, "eglCreatePlatformWindowSurface failed\n");
	}
	printf("eglCreatePlatformWindowSurface successful\n");

	if (eglMakeCurrent(Ri->dpy, Ri->surf_egl, Ri->surf_egl, Ri->ctx) == EGL_FALSE) {
		fprintf(stderr, "eglMakeCurrent failed\n");
	}
	printf("eglMakeCurrent successful\n");

	glClearColor(0, 0, 1, 1);
	glClear(GL_COLOR_BUFFER_BIT);

	if (eglSwapBuffers(Ri->dpy, Ri->surf_egl) == EGL_FALSE) {
		fprintf(stderr, "eglSwapBuffers failed\n");
	}
	printf("eglSwapBuffers successful\n");
		
	return 0;
}

int destroy_renderer(struct renderer_info_t *Ri) {
	if (eglMakeCurrent(Ri->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) == EGL_FALSE) {
		fprintf(stderr, "eglMakeCurrent failed\n");
	}
	printf("eglMakeCurrent successful\n");
	
	gbm_surface_release_buffer(Ri->surf_gbm, Ri->bo);
	printf("surf: %i\n", eglDestroySurface(Ri->dpy, Ri->surf_egl));
	gbm_surface_destroy(Ri->surf_gbm);

	if (eglMakeCurrent(EGL_NO_DISPLAY, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) == EGL_FALSE) {
		fprintf(stderr, "eglMakeCurrent failed\n");
	}
	printf("eglMakeCurrent successful\n");

	printf("ctx: %i\n", eglDestroyContext(Ri->dpy, Ri->ctx));
	printf("dpy: %i\n", eglTerminate(Ri->dpy));
	gbm_device_destroy(Ri->gbm);
	free(Ri);
	return 0;
}
