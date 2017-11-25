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

struct info2_t *create_framebuffer(struct info_t *info, int fd) {
	struct info2_t *info2 = malloc(sizeof(*info2));

	info2->gbm = gbm_create_device(fd);
	if (info2->gbm == NULL) {
		fprintf(stderr, "gbm_create_device failed\n");
	}
	printf("\ngbm_create_device successful\n");

	int ret = gbm_device_is_format_supported(info2->gbm, GBM_BO_FORMAT_XRGB8888,
	GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
	if (!ret) {
		fprintf(stderr, "format unsupported\n");
		return NULL;
	}
	printf("format supported\n");

	info2->surf = gbm_surface_create(info2->gbm, info->mode.hdisplay,
	info->mode.vdisplay, GBM_BO_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT |
	GBM_BO_USE_RENDERING);
	if (info2->surf == NULL) {
		fprintf(stderr, "gbm_surface_create failed\n");
	}
	printf("gbm_surface_create successful\n");

	return info2;
}

void destroy_framebuffer(struct info2_t *info2) {
	gbm_device_destroy(info2->gbm);
}

int display(struct info_t *info, int fd, struct info2_t *info2) {
	int ret;
	info2->bo = gbm_surface_lock_front_buffer(info2->surf);
	if (info2->bo == NULL) {
		fprintf(stderr, "gbm_surface_lock_front_buffer failed\n");
		return -1;
	}
	printf("\ngbm_surface_lock_front_buffer successful\n");

	uint32_t fb_id;
	uint32_t width = gbm_bo_get_width(info2->bo);
	uint32_t height = gbm_bo_get_height(info2->bo);
	uint32_t handle = gbm_bo_get_handle(info2->bo).u32;
	uint32_t stride = gbm_bo_get_stride(info2->bo);
	ret = drmModeAddFB(fd, width, height, 24, 32, stride, handle, &fb_id);
	if (ret) {
		printf("drmModeAddFB failed\n");
	}
	printf("drmModeAddFB successful\n");
		
	ret = drmModeSetCrtc(fd, info->crtc_id, fb_id, 0, 0, &info->connector_id, 1,
	&info->mode);
	if (ret) {
		printf("drmModeSetCrtc failed\n");
	}
	printf("drmModeSetCrtc successful\n");

	sleep(3);

/*	ret = drmModeRmFB(fd, fb_id);
	if (ret) {
		printf("drmModeRmFB failed\n");
	}
	printf("drmModeRmFB successful\n");*/

	return 0;
}

struct info_t *display_info(int fd)
{
	drmModeRes *res;
	int n_conn;
	uint32_t type;

	struct info_t *const info = malloc(sizeof(*info));

	res = drmModeGetResources(fd);
	if (res == NULL) {
		fprintf(stderr, "drmModeGetResources failed\n");
		return NULL;
	}
	
	n_conn = res->count_connectors;

	for (int i=0; i<n_conn; ++i) {
		drmModeConnector *conn = drmModeGetConnector(fd, res->connectors[i]);
		if (conn->connection == DRM_MODE_CONNECTED) {
			drmModeEncoder *enc = drmModeGetEncoder(fd, conn->encoder_id);
			info->crtc_id = enc->crtc_id;
			info->connector_id = conn->connector_id;
			type = conn->connector_type;
			info->mode = conn->modes[0];
			drmModeFreeEncoder(enc);
			drmModeFreeConnector(conn);
			break;
		}
		drmModeFreeConnector(conn);
	}

	drmModeFreeResources(res);

	printf("crtc_id: %"PRIu32"\n", info->crtc_id);
	printf("connector_id: %"PRIu32" (%s)\n", info->connector_id,
	conn_get_name(type));
	printf("mode: %s\n", info->mode.name);

	return info;
/*	printf("Scanning connectors:\n");
	for (int i=0; i<n_conn; ++i) {
		drmModeConnector *conn = drmModeGetConnector(fd, res->connectors[i]);
		printf("encoder_id: %"PRIu32"\n", conn->encoder_id);
		printf("connector_type: %s\n", conn_get_name(conn->connector_type));
		printf("status: %s\n", conn_get_connection(conn->connection));
		printf("n_enc: %i\n", conn->count_encoders);

		printf("count_modes: %i\n", conn->count_modes);
		for (int j=0; j<conn->count_modes; ++j) {
			printf("	%s\n", conn->modes[i].name);
		}
		drmModeFreeConnector(conn);
	}

	printf("Scanning encoders:\n");
	for (int i=0; i<n_enc; i++) {
		drmModeEncoder *enc = drmModeGetEncoder(fd, res->encoders[i]);
		printf("encoder_id: %"PRIu32"\n", enc->encoder_id);
		printf("encoder_type: %"PRIu32"\n", enc->encoder_type);
		printf("crtc_id: %"PRIu32"\n", enc->crtc_id);
		drmModeFreeEncoder(enc);
	}

	printf("Scanning crtcs:\n");
	for (int i=0; i<n_crtc; ++i) {
		drmModeCrtc *crtc = drmModeGetCrtc(fd, res->crtcs[i]);
		printf("crtc_id: %"PRIu32"\n", crtc->crtc_id);
		printf("mode: %s valid: %i\n", crtc->mode.name, crtc->mode_valid);
		drmModeFreeCrtc(crtc);
	}*/
}
