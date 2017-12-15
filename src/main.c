#define _POSIX_C_SOURCE 200809L

#include <backend.h>
#include <renderer.h>

#include <stdio.h> //printf
#include <stdlib.h>
#include <sys/epoll.h> //epoll
#include <time.h> //time

int main() {
	drm* drm_state = drm_create();
	input* input_state = input_create();

	int gpu_fd = drm_get_fd(drm_state);
	int input_fd = input_get_fd(input_state);

	renderer* renderer_state = renderer_create(drm_get_gbm(drm_state),
	drm_get_surf(drm_state));

	renderer_render(renderer_state, 0);
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

	renderer_render(renderer_state, 0);
	drm_pageflip(drm_state);

	time_t start, cur;
	time(&start);

	for (;;) {
		int n = epoll_wait(epfd, events, maxevents, -1);

		if (n < 0)
			continue;
		for (int i=0; i<n; i++) {
			if (events[i].data.fd == input_fd) {
				if (input_handle_event(input_state) == 1)
					goto end;
			} else if (events[i].data.fd == gpu_fd) {
				renderer_render(renderer_state, 0);
				drm_handle_event(drm_state);
			}
		}
	}

end:
	renderer_destroy(renderer_state);

	input_destroy(input_state);
	drm_destroy(drm_state);

	return EXIT_SUCCESS;
}
