#define _POSIX_C_SOURCE 200809L

#include <mysession.h>
#include <mydrm.h>
#include <myegl.h>

#include <stdlib.h>

int main() {
	int major, minor;
	get_boot_gpu(&major, &minor);
	struct session_info_t *session_info = create_session_info();

	take_control(session_info);
	int fd = take_device(session_info, major, minor);

	struct drm_info_t *drm_info = display_info(fd);

	struct renderer_info_t *renderer_info = create_renderer(drm_info, fd);

	drm_modeset(drm_info, renderer_info, fd);

	destroy_renderer(renderer_info);

	drm_cleanup(drm_info, fd);

	release_device(session_info, fd);
	release_control(session_info);

	destroy_session_info(session_info);

	return EXIT_SUCCESS;
}
