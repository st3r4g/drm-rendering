#define _POSIX_C_SOURCE 200809L

#include <mysession.h>
#include <mydrm.h>
#include <myegl.h>

#include <stdlib.h>

int main() {
	struct session_info_t *session_info = create_session_info();

	take_control(session_info);
	int fd = take_device(session_info, "/dev/dri/card0");

	struct info_t *info = display_info(fd);
	struct info2_t *info2 = create_framebuffer(info, fd);

//	struct egl_info_t *egl_info = 
	create_gles_context(info2);

	display(info, fd, info2);

//	destroy_gles_context(egl_info, info2);
	
//	destroy_framebuffer(info2);

	release_device(session_info, fd);
	release_control(session_info);

	destroy_session_info(session_info);

	return EXIT_SUCCESS;
}
