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
/*	int ret;
	
	ret = drmModeAddFB(fd, Ri->width, Ri->height, 24, 32, Ri->stride,
	Ri->handle, &Di->fb_id);

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
*/
	return 0;
}

static void page_flip_event_handler(int fd, unsigned int frame, unsigned int
sec, unsigned int usec, void *data) {
}

int drm_handle_event(int fd) {
	drmEventContext ev;
	ev.version = 2;
	ev.page_flip_handler = page_flip_event_handler;
	drmHandleEvent(fd, &ev);
	return 0;
}

int drm_pageflip(struct drm_info_t *Di, struct renderer_info_t *Ri, int fd) {
	if (Di->n_fb_id == 2) {
		drmModeRmFB(fd, Di->fb_id[0]);
		Di->n_fb_id--;
		Di->fb_id[0] = Di->fb_id[1];
	}

	int ret = drmModeAddFB(fd, Ri->width, Ri->height, 24, 32, Ri->stride,
	Ri->handle, &Di->fb_id[Di->n_fb_id]);
	if (ret) {
		printf("drmModeAddFB failed\n");
	}
	Di->n_fb_id++;
	
	if (Di->n_fb_id == 1) {
		ret = drmModeSetCrtc(fd, Di->crtc_id, Di->fb_id[Di->n_fb_id-1], 0, 0, 
		&Di->connector_id, 1, &Di->mode);
		if (ret) {
			printf("drmModeSetCrtc failed\n");
		}
		printf("drmModeSetCrtc successful\n");
	} else {
		drmModePageFlip(fd, Di->crtc_id, Di->fb_id[Di->n_fb_id-1],
		DRM_MODE_PAGE_FLIP_EVENT, NULL);
		if (ret) {
			printf("drmModePageFlip failed\n");
		}
	}

	return 0;
}

int drm_restore(struct drm_info_t *Di, int fd) {
	int ret = drmModeSetCrtc(fd, Di->crtc_id, Di->old_fb_id, 0, 0, &Di->connector_id, 1,
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
	I->n_fb_id = 0;

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
	printf("Running on %s@%s\n", conn_get_name(type), I->mode.name);
	return I;
}

int drm_cleanup(struct drm_info_t *Di, int fd) {
	drmModeRmFB(fd, Di->fb_id[0]);
	drmModeRmFB(fd, Di->fb_id[1]);
	free(Di);
	return 0;
}
