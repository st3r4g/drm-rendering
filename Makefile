all:
	gcc -I /usr/include/libdrm modeset.c -l drm
	gcc egl.c -l EGL
file:
	gcc file.c -l systemd
