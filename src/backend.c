#include <backend.h>

#include <session.h>

#include <errno.h> // errno
#include <fcntl.h> // open
#include <stdio.h> // printf
#include <string.h> // strerror
#include <unistd.h> // close

#include <sys/stat.h>
#include <sys/sysmacros.h>

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
	int n_fb;
};

// GBM formats are either GBM_BO_FORMAT_XRGB8888 or GBM_BO_FORMAT_ARGB8888
static const int COLOR_DEPTH = 24;
static const int BIT_PER_PIXEL = 32;

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

void get_boot_gpu(int *major, int *minor) {
	struct udev* udev = udev_new();
	if (udev == NULL) {
		fprintf(stderr, "udev_new failed");
	}

	struct udev_enumerate *udev_enum = udev_enumerate_new(udev);
	if (udev_enum == NULL) {
		fprintf(stderr, "udev_enumerate_new failed");
	}

	udev_enumerate_add_match_subsystem(udev_enum, "drm");
	udev_enumerate_add_match_sysname(udev_enum, "card[0-9]");
	udev_enumerate_scan_devices(udev_enum);

	struct udev_list_entry *entry;
	udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(udev_enum)) {
		const char *syspath = udev_list_entry_get_name(entry);
		struct udev_device *gpu = udev_device_new_from_syspath(udev, syspath);
		printf("Found gpu: %s\n", udev_device_get_sysname(gpu)); 

		struct udev_device *pci =
		udev_device_get_parent_with_subsystem_devtype(gpu, "pci", NULL);

		const char *boot = udev_device_get_sysattr_value(pci, "boot_vga");
		if (strcmp(boot, "1") == 0) {
			*major = atoi(udev_device_get_property_value(gpu, "MAJOR"));
			*minor = atoi(udev_device_get_property_value(gpu, "MINOR"));
			printf("using this gpu for rendering\n");
			udev_device_unref(gpu);
			break;
		}
		udev_device_unref(gpu);
	}
	udev_enumerate_unref(udev_enum);
	udev_unref(udev);
}

drm *drm_create(session* session_state)
{
	drm *state = malloc(sizeof(drm));
	int major, minor;
	get_boot_gpu(&major, &minor);
	state->fd = session_take_device(session_state, major, minor);
	state->n_bo = 0;
	state->n_fb = 0;
	
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
	state->n_bo++;

	uint32_t width = gbm_bo_get_width(state->bo[state->n_bo-1]);
	uint32_t height = gbm_bo_get_height(state->bo[state->n_bo-1]);
	uint32_t stride = gbm_bo_get_stride(state->bo[state->n_bo-1]);
	uint32_t handle = gbm_bo_get_handle(state->bo[state->n_bo-1]).u32;

	if (state->n_fb == 2) {
		drmModeRmFB(state->fd, state->fb_id[0]);
		state->n_fb--;
		state->fb_id[0] = state->fb_id[1];
	}

	int ret = drmModeAddFB(state->fd, width, height, COLOR_DEPTH,
	BIT_PER_PIXEL, stride, handle, &state->fb_id[state->n_fb]);
	if (ret) {
		printf("drmModeAddFB failed\n");
	}
	state->n_fb++;
	
	if (state->n_fb == 1) {
//		perform a SetCrtc if it's the first call
		ret = drmModeSetCrtc(state->fd, state->crtc_id,
		state->fb_id[state->n_fb-1], 0, 0, &state->connector_id, 1,
		&state->mode);
		if (ret) {
			fprintf(stderr, "drmModeSetCrtc failed\n");
		}
		printf("\ndrmModeSetCrtc successful\n");
	} else {
//		otherwise perform a PageFlip
		drmModePageFlip(state->fd, state->crtc_id,
		state->fb_id[state->n_fb-1], DRM_MODE_PAGE_FLIP_EVENT,
		state);
		if (ret) {
			fprintf(stderr, "drmModePageFlip failed\n");
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
	int ret = drmModeSetCrtc(state->fd, state->crtc_id, state->old_fb_id,
	0, 0, &state->connector_id, 1, &state->mode);
	if (ret) {
		printf("drmModeSetCrtc failed\n");
	}
	printf("\ndrmModeSetCrtc successful\n");
	
	if (state->n_fb == 2) {
		drmModeRmFB(state->fd, state->fb_id[0]);
		drmModeRmFB(state->fd, state->fb_id[1]);
	} else if (state->n_fb == 1) {
		drmModeRmFB(state->fd, state->fb_id[0]);
	}
	state->n_fb = 0;

	gbm_surface_destroy(state->surf);
	printf("\ngbm_surface_destroy\n");
	gbm_device_destroy(state->gbm);
	printf("gbm_device_destroy\n");

	free(state);
	return 0;
}

//input

struct _input {
	struct libinput *li;
	unsigned char keys;
	double dx;
	double dy;
};

const unsigned char KEY_LEFT = 0x1;
const unsigned char KEY_DOWN = 0x2;
const unsigned char KEY_RIGHT = 0x4;
const unsigned char KEY_UP = 0x8;

const unsigned char KEY_ESC = 0x80;

static int open_restricted(const char *path, int flags, void *user_data)
{
	session *session_state = user_data;
	struct stat st;
	if (stat(path, &st) < 0) {
		fprintf(stderr, "stat failed\n");
	}
	
	int fd = session_take_device(session_state, major(st.st_rdev),
	minor(st.st_rdev));

	if (fd < 0)
		fprintf(stderr, "Failed to open %s (%s)\n", path,
		strerror(errno));

	return fd < 0 ? -errno : fd;
}

static void close_restricted(int fd, void *user_data)
{
	session *session_state = user_data;
	struct stat st;
	if (fstat(fd, &st) < 0) {
		fprintf(stderr, "fstat failed\n");
	}

	session_release_device(session_state, major(st.st_rdev),
	minor(st.st_rdev));
}

static const struct libinput_interface interface = {
	.open_restricted = open_restricted,
	.close_restricted = close_restricted
};

input *input_create(session* session_state) {
	input *state = malloc(sizeof(input));
	state->keys = 0;
	state->dx = 0.0;
	state->dy = 0.0;

	struct udev *udev = udev_new();
	state->li = libinput_udev_create_context(&interface, session_state, udev);

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
	while ((ev = libinput_get_event(state->li))) {
		struct libinput_event_keyboard *key_ev;
		struct libinput_event_pointer *pointer_ev;
		uint32_t keycode;
		switch (libinput_event_get_type(ev)) {
		case LIBINPUT_EVENT_KEYBOARD_KEY:
			key_ev = libinput_event_get_keyboard_event(ev);
			keycode = libinput_event_keyboard_get_key(key_ev);
			if (keycode == 105)
				state->keys ^= KEY_LEFT;
			if (keycode == 108)
				state->keys ^= KEY_DOWN;
			if (keycode == 106)
				state->keys ^= KEY_RIGHT;
			if (keycode == 103)
				state->keys ^= KEY_UP;
			if (keycode == 1)
				state->keys ^= KEY_ESC;
			break;
		case LIBINPUT_EVENT_POINTER_MOTION:
			pointer_ev = libinput_event_get_pointer_event(ev);
			state->dx += libinput_event_pointer_get_dx(pointer_ev);
			state->dy += libinput_event_pointer_get_dy(pointer_ev);
		default:
			;
		}
	}
	return 0;
}

int input_get_keystate_left(input *state)
{
	return (state->keys & KEY_LEFT);
}

int input_get_keystate_down(input *state)
{
	return (state->keys & KEY_DOWN) >> 1;
}

int input_get_keystate_right(input *state)
{
	return (state->keys & KEY_RIGHT) >> 2;
}

int input_get_keystate_up(input *state)
{
	return (state->keys & KEY_UP) >> 3;
}

int input_get_keystate_esc(input *state)
{
	return (state->keys & KEY_ESC) >> 7;
}

double input_get_dx(input *state)
{
	return state->dx;
}

double input_get_dy(input *state)
{
	return state->dy;
}

void input_reset_dxdy(input *state)
{
	state->dx = state->dy = 0.0;
}

int input_destroy(input *state)
{
	free(state);
	return 0;
}
