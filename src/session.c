#include <session.h>

#include <errno.h>  // errno
#include <stdio.h>  // printf
#include <stdlib.h> // malloc
#include <string.h> // strerror

#include <systemd/sd-login.h>
#include <systemd/sd-bus.h>

// singly linked list to store messages that can't be unref'd immediately
// (when TakeDevice messages are unref'd the associated fds are closed)
struct message_list {
	sd_bus_message *msg;
	struct message_list *next;
};

struct _session {
	char *seat;
	char *object;
	sd_bus *bus;
	struct message_list *head;
};

session *session_create() {
	int r = 0;
	char *id = NULL;

	session* state = malloc(sizeof(session));
	if (!state) {
		fprintf(stderr, "FAILED malloc (%s)\n", strerror(errno));
		return NULL;
	}
	state->seat = NULL;
	state->object = NULL;
	state->bus = NULL;
	state->head = NULL;

	r = sd_pid_get_session(0, &id);
	if (r < 0) {
		fprintf(stderr, "FAILED sd_pid_get_session (%s)\n",
		strerror(-r));
		return NULL;
	}

	r = sd_session_get_seat(id, &state->seat);
	if (r < 0) {
		fprintf(stderr, "FAILED sd_session_get_seat (%s)\n",
		strerror(-r));
		free(id);
		return NULL;
	}

	r = sd_bus_default_system(&state->bus);
	if (r < 0) {
		fprintf(stderr, "FAILED sd_bus_default_system (%s)\n",
		strerror(-r));
		free(state->seat), free(id);
		return NULL;
	}
	
	sd_bus_message *msg = NULL;
	sd_bus_error error = SD_BUS_ERROR_NULL;
	r = sd_bus_call_method(state->bus, "org.freedesktop.login1",
	"/org/freedesktop/login1", "org.freedesktop.login1.Manager",
	"GetSession", &error, &msg, "s", id);
	if (r < 0) {
		fprintf(stderr, "FAILED GetSession (%s)\n", strerror(-r));
		sd_bus_unref(state->bus), free(state->seat), free(id);
		return NULL;
	}
	const char *str;
	r = sd_bus_message_read(msg, "o", &str);
	if (r < 0) {
		fprintf(stderr, "ERR on sd_bus_message_read: %s\n",
		strerror(-r));
		sd_bus_error_free(&error), sd_bus_message_unref(msg);
		sd_bus_unref(state->bus), free(state->seat), free(id);
		return NULL;
	}
	state->object = malloc((strlen(str)+1)*sizeof(char));
	strcpy(state->object, str);
	sd_bus_error_free(&error), sd_bus_message_unref(msg);

	return state;
}

int session_take_control(session *state) {
	int r;
	sd_bus_message *msg = NULL;
	sd_bus_error error = SD_BUS_ERROR_NULL;

	r = sd_bus_call_method(state->bus, "org.freedesktop.login1",
	state->object, "org.freedesktop.login1.Session", "TakeControl", &error,
	&msg, "b", 0);
	if (r < 0) {
		fprintf(stderr, "FAILED TakeControl (%s)\n", error.message);
		sd_bus_error_free(&error), sd_bus_message_unref(msg);
		return -1;
	}
	sd_bus_error_free(&error), sd_bus_message_unref(msg);

	return 0;
}

int session_take_device(session *state, int major, int minor) {
	int r;
	sd_bus_message *msg = NULL;
	sd_bus_error error = SD_BUS_ERROR_NULL;
	int fd, paused;

	r = sd_bus_call_method(state->bus, "org.freedesktop.login1",
	state->object, "org.freedesktop.login1.Session", "TakeDevice", &error,
	&msg, "uu", major, minor);
	if (r < 0) {
		fprintf(stderr, "FAILED TakeDevice (%s)\n", error.message);
		sd_bus_error_free(&error);
		return -1;
	}
	struct message_list *node = malloc(sizeof(*node));
	node->msg = msg;
	node->next = state->head;
	state->head = node;
	sd_bus_error_free(&error);
	r = sd_bus_message_read(msg, "hb", &fd, &paused);
	if (r < 0) {
		fprintf(stderr, "FAILED sd_bus_message_read: (%s)\n",
		strerror(-r));
		return -1;
	}

	return fd;
}

int session_release_device(session *state, int major, int minor) {
	int r;
	sd_bus_message *msg = NULL;
	sd_bus_error error = SD_BUS_ERROR_NULL;

	r = sd_bus_call_method(state->bus, "org.freedesktop.login1",
	state->object, "org.freedesktop.login1.Session", "ReleaseDevice",
	&error, &msg, "uu", major, minor);
	if (r < 0) {
		fprintf(stderr, "FAILED ReleaseDevice (%s)\n", error.message);
		sd_bus_error_free(&error), sd_bus_message_unref(msg);
		return -1;
	}
	sd_bus_error_free(&error), sd_bus_message_unref(msg);

	return 0;
}

int session_release_control(session *state) {
	int r;
	sd_bus_message *msg = NULL;
	sd_bus_error error = SD_BUS_ERROR_NULL;

	r = sd_bus_call_method(state->bus, "org.freedesktop.login1",
	state->object, "org.freedesktop.login1.Session", "ReleaseControl",
	&error, &msg, "");
	if (r < 0) {
		fprintf(stderr, "FAILED ReleaseControl (%s)\n", error.message);
		sd_bus_error_free(&error), sd_bus_message_unref(msg);
		return -1;
	}
	sd_bus_error_free(&error), sd_bus_message_unref(msg);

	while(state->head) {
		sd_bus_message_unref(state->head->msg);
		state->head = state->head->next;
	}

	return 0;
}

void session_destroy(session *state) {
	sd_bus_unref(state->bus);
	free(state->object);
	free(state->seat);
	free(state);
}
