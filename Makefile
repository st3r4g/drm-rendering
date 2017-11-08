all:
	gcc -I /usr/include/libdrm modeset.c -l drm
