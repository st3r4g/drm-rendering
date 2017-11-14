#include <mysession.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <sys/stat.h>
#include <sys/sysmacros.h>

#include <systemd/sd-login.h>
#include <systemd/sd-bus.h>

int take_device(struct session_info_t *S, const char *path) {
	int ret;
	struct stat st;
	sd_bus_message *msg = NULL;
	sd_bus_error error = SD_BUS_ERROR_NULL;
	int fd, paused;

	ret = stat(path, &st);
	if (ret < 0) {
		fprintf(stderr, "ERR on stat(%s): %s", path, strerror(errno));
		goto end0;
	}

	ret = sd_bus_call_method(S->bus, "org.freedesktop.login1", S->object,
	"org.freedesktop.login1.Session", "TakeDevice", &error, &msg, "uu",
	major(st.st_rdev), minor(st.st_rdev));
	if (ret < 0) {
		fprintf(stderr, "ERR on TakeDevice(%s): %s", path, error.message);
		goto end1;
	}

	ret = sd_bus_message_read(msg, "hb", &fd, &paused);
	if (ret < 0) {
		fprintf(stderr, "ERR on message_read(%s): %s", path, strerror(-ret));
		goto end1;
	}

end1:
	sd_bus_error_free(&error);
	sd_bus_message_unref(msg);
end0:
	return ret < 0 ? EXIT_FAILURE : fd;
}

struct session_info_t *create_session_info() {
	int ret = 0;

	struct session_info_t *session_info = calloc(1, sizeof(*session_info));
	if (!session_info) {
		fprintf(stderr, "ERR: session_info calloc: %s\n", strerror(-ret));
		return NULL;
	}

	session_info->id = NULL;
	session_info->seat = NULL;

	ret = sd_pid_get_session(0, &session_info->id);
	if (ret < 0) {
		fprintf(stderr, "err: %s\n", strerror(-ret));
		goto end;
	}
	printf("session id: %s\n", session_info->id);

	ret = sd_session_get_vt(session_info->id, &session_info->vt);
	if (ret < 0) {
		fprintf(stderr, "err: %s\n", strerror(-ret));
		goto end;
	}
	printf("session vt: %i\n", session_info->vt);

	ret = sd_session_get_seat(session_info->id, &session_info->seat);
	if (ret < 0) {
		fprintf(stderr, "err: %s\n", strerror(-ret));
		goto end;
	}
	printf("session seat: %s\n", session_info->seat);

end:
	free(session_info->seat);
	free(session_info->id);

	return ret < 0 ? NULL : session_info;
}

int destroy_session_info(struct session_info_t *session_info) {
	free(session_info);
	return 0;
}
