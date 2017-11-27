#ifndef MYDRM_H
#define MYDRM_H

#include <xf86drm.h>
#include <xf86drmMode.h>

struct drm_info_t {
	uint32_t crtc_id;
	uint32_t connector_id;
	uint32_t old_fb_id;
	uint32_t fb_id;
	drmModeModeInfo mode;
};

#include <myrenderer.h>

struct drm_info_t *display_info(int fd);

int drm_modeset(
	struct drm_info_t *info,
	struct renderer_info_t *renderer_info,
	int fd
);

int drm_cleanup(struct drm_info_t *drm_info, int fd);

#endif
