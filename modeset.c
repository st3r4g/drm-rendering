#include <fcntl.h> // for open
#include <unistd.h> // for close
#include <stdio.h> // for fprintf
#include <stdint.h> // for uint_64

#include <errno.h>

#include <xf86drm.h>
//#include <xf86drmMode.h>

int modeset_open(const char *node)
{
	int fd, ret;
	uint64_t has_dumb;

	fd = open(node, O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		ret = -errno;
		fprintf(stderr, "cannot open '%s': %m\n", node);
		return ret;
	}

	if (drmGetCap(fd, DRM_CAP_DUMB_BUFFER, &has_dumb) < 0 || !has_dumb) {
		fprintf(stderr, "drm device '%s' does not support dumb buffers\n",
		node);
		close(fd);
		return -EOPNOTSUPP;
	}
	
	return fd;
}

int main()
{
	int fd;
	return modeset_open("/dev/dri/card0");
}
