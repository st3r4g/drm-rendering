#define _POSIX_C_SOURCE 200809L

#include <session.h>

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#include <systemd/sd-login.h>
#include <systemd/sd-bus.h>

struct _session {
	char *id;
	unsigned int vt;
	char *seat;
	sd_bus *bus;
	char *object;
	sd_bus_message *TD_msg; //TODO: find better solution
};

int session_take_device(session *state, int major, int minor) {
	int ret;
	sd_bus_error error = SD_BUS_ERROR_NULL;
	int fd, paused;

	ret = sd_bus_call_method(state->bus, "org.freedesktop.login1", state->object,
	"org.freedesktop.login1.Session", "TakeDevice", &error, &state->TD_msg, "uu",
	major, minor);
	if (ret < 0) {
		fprintf(stderr, "ERR on TakeDevice: %s\n", error.message);
		goto end;
	}

	ret = sd_bus_message_read(state->TD_msg, "hb", &fd, &paused);
	if (ret < 0) {
		fprintf(stderr, "ERR on message_read: %s\n", strerror(-ret));
	}
	printf("opened, paused: %i\n", paused);

end:
	sd_bus_error_free(&error);

	return ret < 0 ? EXIT_FAILURE : fd;
}

int session_release_device(session *S, int major, int minor) {
	int ret;
	struct stat st;
	sd_bus_message *msg = NULL;
	sd_bus_error error = SD_BUS_ERROR_NULL;

	ret = sd_bus_call_method(S->bus, "org.freedesktop.login1", S->object,
	"org.freedesktop.login1.Session", "ReleaseDevice", &error, &msg, "uu",
	major, minor);
	if (ret < 0) {
		fprintf(stderr, "ERR on ReleaseDevice: %s\n", error.message);
		goto end;
	}

end:
	sd_bus_error_free(&error);
	sd_bus_message_unref(msg);
	sd_bus_message_unref(S->TD_msg); //TODO: find better solution

	return ret < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

int session_take_control(session *S) {
	int ret;
	sd_bus_message *msg = NULL;
	sd_bus_error error = SD_BUS_ERROR_NULL;

	ret = sd_bus_call_method(S->bus, "org.freedesktop.login1", S->object,
	"org.freedesktop.login1.Session", "TakeControl", &error, &msg, "b", 0);
	if (ret < 0) {
		fprintf(stderr, "ERR on TakeControl(false): %s\n", error.message);
	}
	sd_bus_error_free(&error);
	sd_bus_message_unref(msg);

	return ret < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

int session_release_control(session *S) {
	int ret;
	sd_bus_message *msg = NULL;
	sd_bus_error error = SD_BUS_ERROR_NULL;

	ret = sd_bus_call_method(S->bus, "org.freedesktop.login1", S->object,
	"org.freedesktop.login1.Session", "ReleaseControl", &error, &msg, "");
	if (ret < 0) {
		fprintf(stderr, "ERR on ReleaseControl(): %s\n", error.message);
	}
	sd_bus_error_free(&error);
	sd_bus_message_unref(msg);

	return ret < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

session *session_create() {
	int ret = 0;

	session* state = malloc(sizeof(session));
	if (!state) {
		fprintf(stderr, "ERR: state calloc: %s\n", strerror(-ret));
		return NULL;
	}

	state->id = NULL;
	state->seat = NULL;
	state->bus = NULL;
	state->TD_msg = NULL;

	ret = sd_pid_get_session(0, &state->id);
	if (ret < 0) {
		fprintf(stderr, "err: %s\n", strerror(-ret));
		session_destroy(state);
		return NULL;
	}
	printf("session id: %s\n", state->id);

	ret = sd_session_get_vt(state->id, &state->vt);
	if (ret < 0) {
		fprintf(stderr, "err: %s\n", strerror(-ret));
		session_destroy(state);
		return NULL;
	}
	printf("session vt: %i\n", state->vt);

	ret = sd_session_get_seat(state->id, &state->seat);
	if (ret < 0) {
		fprintf(stderr, "err: %s\n", strerror(-ret));
		session_destroy(state);
		return NULL;
	}
	printf("session seat: %s\n", state->seat);

	ret = sd_bus_default_system(&state->bus);
	if (ret < 0) {
		fprintf(stderr, "err: %s\n", strerror(-ret));
		session_destroy(state);
		return NULL;
	}
	
	sd_bus_message *msg = NULL;
	sd_bus_error error = SD_BUS_ERROR_NULL;
	ret = sd_bus_call_method(state->bus, "org.freedesktop.login1",
	"/org/freedesktop/login1", "org.freedesktop.login1.Manager", "GetSession",
	&error, &msg, "s", state->id);
	if (ret < 0) {
		fprintf(stderr, "err: %s\n", strerror(-ret));
		sd_bus_error_free(&error);
		session_destroy(state);
		return NULL;
	}
	const char *temp;
	ret = sd_bus_message_read(msg, "o", &temp);
	state->object = strdup(temp);
	sd_bus_message_unref(msg);

	return state;
}

int session_destroy(session *state) {
	free(state->object);
	sd_bus_unref(state->bus);
	free(state->seat);
	free(state->id);
	free(state);
	return 0;
}
