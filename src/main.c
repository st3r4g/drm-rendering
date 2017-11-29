#define _POSIX_C_SOURCE 200809L

#include <mysession.h>
#include <mydrm.h>
#include <myrenderer.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <time.h>

int main() {
	int major, minor;
	get_boot_gpu(&major, &minor);
	struct session_info_t *session_info = create_session_info();

	take_control(session_info);
	int fd = take_device(session_info, major, minor);

	struct drm_info_t *drm_info = display_info(fd);

	struct renderer_info_t *renderer_info = create_renderer(drm_info, fd);

	time_t start, cur;
	time(&start);

	render(renderer_info, 0);

	drm_pageflip(drm_info, renderer_info, fd);
	
	while (time(&cur)-start < 5) {
		fd_set fds;
		struct timeval timeout = { .tv_sec = 3, .tv_usec = 0 };

		FD_ZERO(&fds);
		FD_SET(0, &fds);
		FD_SET(fd, &fds);

		render(renderer_info, cur-start);
		drm_pageflip(drm_info, renderer_info, fd);

		int ret = select(fd+1, &fds, NULL, NULL, &timeout); //POLL

		if (ret <= 0) {
			fprintf(stderr, "select timed out or error %d\n", ret);
			continue;
		} else if (FD_ISSET(0, &fds)) {
			break;
		}
		
		drm_handle_event(fd);
	}

	drm_restore(drm_info, fd);

	destroy_renderer(renderer_info);

	drm_cleanup(drm_info, fd);

	release_device(session_info, fd);
	release_control(session_info);

	destroy_session_info(session_info);

	return EXIT_SUCCESS;
}
