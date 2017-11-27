#include <mydrm.h>

#include <inttypes.h>
#include <stdio.h> // for fprintf
#include <stdint.h> // for uint_64
#include <stdlib.h>
#include <unistd.h> //sleep

#include <errno.h>


const char *conn_get_name(uint32_t type_id) {
	switch (type_id) {
	case DRM_MODE_CONNECTOR_Unknown:     return "Unknown";
	case DRM_MODE_CONNECTOR_VGA:         return "VGA";
	case DRM_MODE_CONNECTOR_DVII:        return "DVI-I";
	case DRM_MODE_CONNECTOR_DVID:        return "DVI-D";
	case DRM_MODE_CONNECTOR_DVIA:        return "DVI-A";
	case DRM_MODE_CONNECTOR_Composite:   return "Composite";
	case DRM_MODE_CONNECTOR_SVIDEO:      return "SVIDEO";
	case DRM_MODE_CONNECTOR_LVDS:        return "LVDS";
	case DRM_MODE_CONNECTOR_Component:   return "Component";
	case DRM_MODE_CONNECTOR_9PinDIN:     return "DIN";
	case DRM_MODE_CONNECTOR_DisplayPort: return "DP";
	case DRM_MODE_CONNECTOR_HDMIA:       return "HDMI-A";
	case DRM_MODE_CONNECTOR_HDMIB:       return "HDMI-B";
	case DRM_MODE_CONNECTOR_TV:          return "TV";
	case DRM_MODE_CONNECTOR_eDP:         return "eDP";
	case DRM_MODE_CONNECTOR_VIRTUAL:     return "Virtual";
	case DRM_MODE_CONNECTOR_DSI:         return "DSI";
	default:                             return "Unknown";
	}
}

const char *conn_get_connection(drmModeConnection connection) {
	switch (connection) {
	case DRM_MODE_CONNECTED:         return "connected";
	case DRM_MODE_DISCONNECTED:      return "disconnected";
	case DRM_MODE_UNKNOWNCONNECTION: return "unknown";
	default:                         return "unknown";
	}
}

int drm_modeset(struct drm_info_t *Di, struct renderer_info_t *Ri, int fd) {
	int ret;
	Ri->bo = gbm_surface_lock_front_buffer(Ri->surf_gbm);
	if (Ri->bo == NULL) {
		fprintf(stderr, "gbm_surface_lock_front_buffer failed\n");
		return -1;
	}
	printf("\ngbm_surface_lock_front_buffer successful\n");

	uint32_t width = gbm_bo_get_width(Ri->bo);
	uint32_t height = gbm_bo_get_height(Ri->bo);
	uint32_t handles[4] = {gbm_bo_get_handle(Ri->bo).u32};
	uint32_t pitches[4] = {gbm_bo_get_stride(Ri->bo)};
	uint32_t offsets[4] = {gbm_bo_get_offset(Ri->bo, 0)};
	uint32_t format = gbm_bo_get_format(Ri->bo);
//	uint32_t handle = gbm_bo_get_handle(info2->bo).u32;
//	uint32_t stride = gbm_bo_get_stride(info2->bo);
//	ret = drmModeAddFB(fd, width, height, 24, 32, stride, handle, fb_id);
	ret = drmModeAddFB2(fd, width, height, format, handles, pitches, offsets,
	&Di->fb_id, 0);

	if (ret) {
		printf("drmModeAddFB failed\n");
	}
	printf("drmModeAddFB successful\n");
		
	ret = drmModeSetCrtc(fd, Di->crtc_id, Di->fb_id, 0, 0, &Di->connector_id, 1,
	&Di->mode);
	if (ret) {
		printf("drmModeSetCrtc failed\n");
	}
	printf("drmModeSetCrtc successful\n");

	sleep(1);

	ret = drmModeSetCrtc(fd, Di->crtc_id, Di->old_fb_id, 0, 0, &Di->connector_id, 1,
	&Di->mode);
	if (ret) {
		printf("drmModeSetCrtc failed\n");
	}
	printf("drmModeSetCrtc successful\n");
	
	return 0;
}

struct drm_info_t *display_info(int fd)
{
	drmModeRes *res;
	uint32_t type;

	struct drm_info_t *const I = malloc(sizeof(*I));

	res = drmModeGetResources(fd);
	if (res == NULL) {
		fprintf(stderr, "drmModeGetResources failed\n");
		return NULL;
	}

	for (int i=0; i<res->count_connectors; ++i) {
		drmModeConnector *conn = drmModeGetConnector(fd, res->connectors[i]);
		if (conn->connection == DRM_MODE_CONNECTED) {
			drmModeEncoder *enc = drmModeGetEncoder(fd, conn->encoder_id);
			I->crtc_id = enc->crtc_id;
			I->connector_id = conn->connector_id;
			type = conn->connector_type;
			I->mode = conn->modes[0];

			drmModeCrtc *crtc = drmModeGetCrtc(fd, I->crtc_id);
			I->old_fb_id = crtc->buffer_id;

			drmModeFreeCrtc(crtc);
			drmModeFreeEncoder(enc);
			drmModeFreeConnector(conn);
			break;
		}
		drmModeFreeConnector(conn);
	}

	drmModeFreeResources(res);
	printf("Running on %s %s\n", conn_get_name(type), I->mode.name);
	return I;
}

int drm_cleanup(struct drm_info_t *Di, int fd) {
	drmModeRmFB(fd, Di->fb_id);
	free(Di);
	return 0;
}
