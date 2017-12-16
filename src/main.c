#define _POSIX_C_SOURCE 200809L

#include <backend.h>
#include <renderer.h>
#include <session.h>

#include <stdio.h> //printf
#include <stdlib.h>
#include <sys/epoll.h> //epoll

int main() {
	session *session_state = session_create();
	session_take_control(session_state);

	drm* drm_state = drm_create(session_state);
	input* input_state = input_create(session_state);

	int gpu_fd = drm_get_fd(drm_state);
	int input_fd = input_get_fd(input_state);

	renderer* renderer_state = renderer_create(drm_get_gbm(drm_state),
	drm_get_surf(drm_state));

	renderer_render(renderer_state, input_state);
	drm_pageflip(drm_state);

	int epfd = epoll_create(1);

	struct epoll_event input_event_ctl;
	input_event_ctl.data.fd = input_fd;
	input_event_ctl.events = EPOLLIN;
	epoll_ctl(epfd, EPOLL_CTL_ADD, input_fd, &input_event_ctl);
	
	struct epoll_event gpu_event_ctl;
	gpu_event_ctl.data.fd = gpu_fd;
	gpu_event_ctl.events = EPOLLIN;
	epoll_ctl(epfd, EPOLL_CTL_ADD, gpu_fd, &gpu_event_ctl);
	
	struct epoll_event *events;
	const int maxevents = 32;
	events = malloc(maxevents*sizeof(*events));

	renderer_render(renderer_state, input_state);
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

	input_destroy(input_state);
	drm_destroy(drm_state);
	
	session_release_control(session_state);
	session_destroy(session_state);

	return EXIT_SUCCESS;
}
