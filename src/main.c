#include <mysession.h>
#include <mydrm.h>
#include <myegl.h>

//TODO: msg containing gpu fd is not unref

int main() {
	struct session_info_t *session_info = create_session_info();

	take_control(session_info);
	int fd = take_device(session_info, "/dev/dri/card0");

	display_info(fd);

	release_device(session_info, fd);
	release_control(session_info);

	destroy_session_info(session_info);
	return 0;
}
