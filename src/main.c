#include <backend.h>
#include <renderer.h>
#include <session.h>

#include <stdio.h>     // printf
#include <stdlib.h>    // malloc
#include <sys/epoll.h> // epoll

int main() {
	int r, status = EXIT_SUCCESS;

	session *session_state = session_create();
	if (!session_state) {
		fprintf(stderr, "FAILED session_create\n");
		status = EXIT_FAILURE;
		return status;
	}

	r = session_take_control(session_state);
	if (r < 0) {
		fprintf(stderr, "FAILED session_take_control\n");
		status = EXIT_FAILURE;
		goto session_destroy;
	}

	drm* drm_state = drm_create(session_state);
	if (!drm_state) {
		fprintf(stderr, "FAILED drm_create\n");
		status = EXIT_FAILURE;
		goto session_release_control;
	}

	input* input_state = input_create(session_state);
	if (!input_state) {
		fprintf(stderr, "FAILED input_create\n");
		status = EXIT_FAILURE;
		goto drm_destroy;
	}

	renderer* renderer_state = renderer_create(drm_get_gbm(drm_state),
	drm_get_surf(drm_state));

//	render something
	renderer_render(renderer_state, input_state);
//	set the crtc
	drm_pageflip(drm_state);

	int epfd = epoll_create(1);

	struct epoll_event input_event_ctl;
	int input_fd = input_get_fd(input_state);
	input_event_ctl.data.fd = input_fd;
	input_event_ctl.events = EPOLLIN;
	epoll_ctl(epfd, EPOLL_CTL_ADD, input_fd, &input_event_ctl);
	
	struct epoll_event gpu_event_ctl;
	int gpu_fd = drm_get_fd(drm_state);
	gpu_event_ctl.data.fd = gpu_fd;
	gpu_event_ctl.events = EPOLLIN;
	epoll_ctl(epfd, EPOLL_CTL_ADD, gpu_fd, &gpu_event_ctl);
	
	struct epoll_event *events;
	const int maxevents = 32;
	events = malloc(maxevents*sizeof(*events));

//	render something
	renderer_render(renderer_state, input_state);
//	schedule the first pageflip
	drm_pageflip(drm_state);

	for (;;) {
		int n = epoll_wait(epfd, events, maxevents, -1);

		if (n < 0)
			continue;
		for (int i=0; i<n; i++) {
			if (events[i].data.fd == input_fd) {
				input_handle_event(input_state);
				if (input_get_keystate_esc(input_state))
					goto end;
			} else if (events[i].data.fd == gpu_fd) {
				renderer_render(renderer_state, input_state);
				drm_handle_event(drm_state);
			}
		}
	}
end:
	renderer_destroy(renderer_state);
input_destroy:
	input_destroy(input_state);
drm_destroy:
	drm_destroy(drm_state);
session_release_control:
	session_release_control(session_state);
session_destroy:
	session_destroy(session_state);

	return status;
}
