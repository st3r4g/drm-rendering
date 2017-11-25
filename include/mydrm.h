#ifndef MYDRM_H
#define MYDRM_H

#include <gbm.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

struct info_t {
	uint32_t crtc_id;
	uint32_t connector_id;
	drmModeModeInfo mode;
};

struct info2_t {
	struct gbm_device *gbm;
	struct gbm_surface *surf;
	struct gbm_bo *bo;
};

struct info_t *display_info(int fd);
struct info2_t *create_framebuffer(struct info_t *info, int fd);
void destroy_framebuffer(struct info2_t *info2);
int display(struct info_t *info, int fd, struct info2_t *info2);
#endif
