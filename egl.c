#include <stdio.h>

#include <EGL/egl.h>

int main()
{
	EGLDisplay eglDpy;
	EGLint major, minor;

	eglDpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	eglInitialize(eglDpy, &major, &minor);

	printf("major: %i\n", major);
	printf("minor: %i\n", minor);

	eglTerminate(eglDpy);
}


