#include <backend.h>

#include <errno.h> // errno
#include <fcntl.h> // open
#include <stdio.h> // printf
#include <string.h> // strerror
#include <unistd.h> // close

#include <libinput.h>

#include <libudev.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

struct _drm {
	int fd;

	uint32_t crtc_id;
	uint32_t connector_id;
	uint32_t old_fb_id;
	drmModeModeInfo mode;

	struct gbm_device *gbm;
	struct gbm_surface *surf;
	struct gbm_bo *bo[2];
	int n_bo;

	uint32_t fb_id[2];
	int n_fb_id;
};

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

drm *drm_create()
{
	drm *state = malloc(sizeof(drm));
	state->fd = drmOpen("i915", 0);
	state->n_bo = 0;
	state->n_fb_id = 0;
	
	drmModeRes *res;
	uint32_t type;

	res = drmModeGetResources(state->fd);
	if (res == NULL) {
		fprintf(stderr, "drmModeGetResources failed\n");
		return NULL;
	}

	for (int i=0; i<res->count_connectors; ++i) {
		drmModeConnector *conn = drmModeGetConnector(state->fd,
		res->connectors[i]);
		if (conn->connection == DRM_MODE_CONNECTED) {
			drmModeEncoder *enc = drmModeGetEncoder(state->fd,
			conn->encoder_id);
			state->crtc_id = enc->crtc_id;
			state->connector_id = conn->connector_id;
			type = conn->connector_type;
			state->mode = conn->modes[0];

			drmModeCrtc *crtc = drmModeGetCrtc(state->fd,
			state->crtc_id);
			state->old_fb_id = crtc->buffer_id;

			drmModeFreeCrtc(crtc);
			drmModeFreeEncoder(enc);
			drmModeFreeConnector(conn);
			break;
		}
		drmModeFreeConnector(conn);
	}

	drmModeFreeResources(res);
	printf("Running on %s@%s\n", conn_get_name(type), state->mode.name);

	state->gbm = gbm_create_device(state->fd);
	if (state->gbm == NULL) {
		fprintf(stderr, "gbm_create_device failed\n");
	}
	printf("\ngbm_create_device successful\n");

	int ret = gbm_device_is_format_supported(state->gbm, GBM_BO_FORMAT_XRGB8888,
	GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
	if (!ret) {
		fprintf(stderr, "format unsupported\n");
		return NULL;
	}
	printf("format supported\n");

	state->surf= gbm_surface_create(state->gbm, state->mode.hdisplay,
	state->mode.vdisplay, GBM_BO_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT |
	GBM_BO_USE_RENDERING);
	if (state->surf == NULL) {
		fprintf(stderr, "gbm_surface_create failed\n");
	}
	printf("gbm_surface_create successful\n");

	return state;
}

int drm_get_fd(drm *state) {
	return state->fd;
}

struct gbm_device *drm_get_gbm(drm *state) {
	return state->gbm;
}


struct gbm_surface *drm_get_surf(drm *state) {
	return state->surf;
}

/*int drm_modeset(struct drm_info_t *Di, struct renderer_info_t *Ri, int fd) {
	int ret;
	
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

	return 0;
}*/

int drm_pageflip(drm* state) {
	if (state->n_bo == 2) {
		gbm_surface_release_buffer(state->surf, state->bo[0]);
		state->n_bo--;
		state->bo[0] = state->bo[1];
	}

	state->bo[state->n_bo] = gbm_surface_lock_front_buffer(state->surf);
	if (state->bo[state->n_bo] == NULL) {
		fprintf(stderr, "gbm_surface_lock_front_buffer failed\n");
		return -1;
	}
	printf("gbm_surface_lock_front_buffer successful\n");
	state->n_bo++;

/*	uint32_t handles[4] = {gbm_bo_get_handle(state->bo).u32};
	uint32_t pitches[4] = {gbm_bo_get_stride(Ri->bo)};
	uint32_t offsets[4] = {gbm_bo_get_offset(Ri->bo, 0)};
	uint32_t format = gbm_bo_get_format(Ri->bo);
	ret = drmModeAddFB2(fd, width, height, format, handles, pitches, offsets,
	&Di->fb_id, 0);*/

	uint32_t width = gbm_bo_get_width(state->bo[state->n_bo-1]);
	uint32_t height = gbm_bo_get_height(state->bo[state->n_bo-1]);
	uint32_t stride = gbm_bo_get_stride(state->bo[state->n_bo-1]);
	uint32_t handle = gbm_bo_get_handle(state->bo[state->n_bo-1]).u32;

	if (state->n_fb_id == 2) {
		drmModeRmFB(state->fd, state->fb_id[0]);
		state->n_fb_id--;
		state->fb_id[0] = state->fb_id[1];
	}

	int ret = drmModeAddFB(state->fd, width, height, 24, 32, stride, handle,
	&state->fb_id[state->n_fb_id]);
	if (ret) {
		printf("drmModeAddFB failed\n");
	}
	state->n_fb_id++;
	
	if (state->n_fb_id == 1) {
		ret = drmModeSetCrtc(state->fd, state->crtc_id,
		state->fb_id[state->n_fb_id-1], 0, 0, &state->connector_id, 1,
		&state->mode);
		if (ret) {
			printf("drmModeSetCrtc failed\n");
		}
		printf("drmModeSetCrtc successful\n");
	} else {
		drmModePageFlip(state->fd, state->crtc_id, state->fb_id[state->n_fb_id-1],
		DRM_MODE_PAGE_FLIP_EVENT, state);
		if (ret) {
			printf("drmModePageFlip failed\n");
		}
	}
	
	
	return 0;
}

static void page_flip_handler(int fd, unsigned int frame, unsigned int
sec, unsigned int usec, void *data) {
	drm *state = data;
	drm_pageflip(state);
}

int drm_handle_event(drm* state) {
	drmEventContext ev;
	ev.version = 2;
	ev.page_flip_handler = page_flip_handler;
	drmHandleEvent(state->fd, &ev);
	return 0;
}

int drm_destroy(drm *state) {
	int ret = drmModeSetCrtc(state->fd, state->crtc_id, state->old_fb_id, 0, 0,
	&state->connector_id, 1, &state->mode);
	if (ret) {
		printf("drmModeSetCrtc failed\n");
	}
	printf("drmModeSetCrtc successful\n");

	drmModeRmFB(state->fd, state->fb_id[0]);
	drmModeRmFB(state->fd, state->fb_id[1]);

	gbm_surface_release_buffer(state->surf, state->bo[0]);
	gbm_surface_release_buffer(state->surf, state->bo[1]);
	state->n_bo = 0;
	gbm_surface_destroy(state->surf);
	gbm_device_destroy(state->gbm);

	free(state);
	return 0;
}

//input

struct _input {
	struct libinput *li;
};

static int open_restricted(const char *path, int flags, void *user_data)
{
	int fd = open(path, flags);

	if (fd < 0)
		fprintf(stderr, "Failed to open %s (%s)\n", path,
		strerror(errno));

	return fd < 0 ? -errno : fd;
}

static void close_restricted(int fd, void *user_data)
{
	close(fd);
}

static const struct libinput_interface interface = {
	.open_restricted = open_restricted,
	.close_restricted = close_restricted
};

input *input_create() {
	input *state = malloc(sizeof(input));

	struct udev *udev = udev_new();
	state->li = libinput_udev_create_context(&interface, 0, udev);

	libinput_udev_assign_seat(state->li, "seat0");

	return state;
}

int input_get_fd(input *state) {
	return libinput_get_fd(state->li);
}

int input_handle_event(input *state)
{
	libinput_dispatch(state->li);
	struct libinput_event *ev;
	int ret = 0;
	while ((ev = libinput_get_event(state->li))) {
		switch (libinput_event_get_type(ev)) {
		case LIBINPUT_EVENT_KEYBOARD_KEY:
			ret = 1;
		default:
			;
		}
	}
	return ret;
}

int input_destroy(input *state)
{
	free(state);
	return 0;
}
