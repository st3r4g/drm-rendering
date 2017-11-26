#define _POSIX_C_SOURCE 200809L

#include <mysession.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <sys/stat.h>
#include <sys/sysmacros.h>

#include <systemd/sd-login.h>
#include <systemd/sd-bus.h>

#include <libudev.h>

void get_boot_gpu(int *major, int *minor) {
	struct udev* udev = udev_new();
	if (udev == NULL) {
		fprintf(stderr, "udev_new failed");
	}

	struct udev_enumerate *udev_enum = udev_enumerate_new(udev);
	if (udev_enum == NULL) {
		fprintf(stderr, "udev_enumerate_new failed");
	}

	udev_enumerate_add_match_subsystem(udev_enum, "drm");
	udev_enumerate_add_match_sysname(udev_enum, "card[0-9]");
	udev_enumerate_scan_devices(udev_enum);

	struct udev_list_entry *entry;
	udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(udev_enum)) {
		const char *syspath = udev_list_entry_get_name(entry);
		struct udev_device *gpu = udev_device_new_from_syspath(udev, syspath);
		printf("Found gpu: %s\n", udev_device_get_sysname(gpu)); 

		struct udev_device *pci =
		udev_device_get_parent_with_subsystem_devtype(gpu, "pci", NULL);

		const char *boot = udev_device_get_sysattr_value(pci, "boot_vga");
		if (strcmp(boot, "1") == 0) {
			*major = atoi(udev_device_get_property_value(gpu, "MAJOR"));
			*minor = atoi(udev_device_get_property_value(gpu, "MINOR"));
			printf("using this gpu for rendering\n");
			udev_device_unref(gpu);
			break;
		}
		udev_device_unref(gpu);
	}
	udev_enumerate_unref(udev_enum);
	udev_unref(udev);
}

int take_device(struct session_info_t *S, int major, int minor) {
	int ret;
	sd_bus_error error = SD_BUS_ERROR_NULL;
	int fd, paused;

	printf("%i, %i\n", major, minor);
	ret = sd_bus_call_method(S->bus, "org.freedesktop.login1", S->object,
	"org.freedesktop.login1.Session", "TakeDevice", &error, &S->TD_msg, "uu",
	major, minor);
	if (ret < 0) {
		fprintf(stderr, "ERR on TakeDevice: %s\n", error.message);
		goto end;
	}

	ret = sd_bus_message_read(S->TD_msg, "hb", &fd, &paused);
	if (ret < 0) {
		fprintf(stderr, "ERR on message_read: %s\n", strerror(-ret));
	}
	printf("opened, paused: %i\n", paused);

end:
	sd_bus_error_free(&error);

	return ret < 0 ? EXIT_FAILURE : fd;
}

int release_device(struct session_info_t *S, int fd) {
	int ret;
	struct stat st;
	sd_bus_message *msg = NULL;
	sd_bus_error error = SD_BUS_ERROR_NULL;

	ret = fstat(fd, &st);
	if (ret < 0) {
		fprintf(stderr, "ERR on fstat(%i): %s\n", fd, strerror(errno));
		goto end;
	}

	ret = sd_bus_call_method(S->bus, "org.freedesktop.login1", S->object,
	"org.freedesktop.login1.Session", "ReleaseDevice", &error, &msg, "uu",
	major(st.st_rdev), minor(st.st_rdev));
	if (ret < 0) {
		fprintf(stderr, "ERR on ReleaseDevice(%i): %s\n", fd, error.message);
		goto end;
	}

end:
	sd_bus_error_free(&error);
	sd_bus_message_unref(msg);
	sd_bus_message_unref(S->TD_msg); //TODO: find better solution

	return ret < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

int take_control(struct session_info_t *S) {
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

int release_control(struct session_info_t *S) {
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

struct session_info_t *create_session_info() {
	int ret = 0;

	struct session_info_t *session_info = calloc(1, sizeof(*session_info));
	if (!session_info) {
		fprintf(stderr, "ERR: session_info calloc: %s\n", strerror(-ret));
		return NULL;
	}

	session_info->id = NULL;
	session_info->seat = NULL;
	session_info->bus = NULL;
	session_info->TD_msg = NULL;

	ret = sd_pid_get_session(0, &session_info->id);
	if (ret < 0) {
		fprintf(stderr, "err: %s\n", strerror(-ret));
		destroy_session_info(session_info);
		return NULL;
	}
	printf("session id: %s\n", session_info->id);

	ret = sd_session_get_vt(session_info->id, &session_info->vt);
	if (ret < 0) {
		fprintf(stderr, "err: %s\n", strerror(-ret));
		destroy_session_info(session_info);
		return NULL;
	}
	printf("session vt: %i\n", session_info->vt);

	ret = sd_session_get_seat(session_info->id, &session_info->seat);
	if (ret < 0) {
		fprintf(stderr, "err: %s\n", strerror(-ret));
		destroy_session_info(session_info);
		return NULL;
	}
	printf("session seat: %s\n", session_info->seat);

	ret = sd_bus_default_system(&session_info->bus);
	if (ret < 0) {
		fprintf(stderr, "err: %s\n", strerror(-ret));
		destroy_session_info(session_info);
		return NULL;
	}
	
	sd_bus_message *msg = NULL;
	sd_bus_error error = SD_BUS_ERROR_NULL;
	ret = sd_bus_call_method(session_info->bus, "org.freedesktop.login1",
	"/org/freedesktop/login1", "org.freedesktop.login1.Manager", "GetSession",
	&error, &msg, "s", session_info->id);
	if (ret < 0) {
		fprintf(stderr, "err: %s\n", strerror(-ret));
		sd_bus_error_free(&error);
		destroy_session_info(session_info);
		return NULL;
	}
	const char *temp;
	ret = sd_bus_message_read(msg, "o", &temp);
	session_info->object = strdup(temp);
	sd_bus_message_unref(msg);
	
	return session_info;
}

int destroy_session_info(struct session_info_t *session_info) {
	free(session_info->object);
	sd_bus_unref(session_info->bus);
	free(session_info->seat);
	free(session_info->id);
	free(session_info);
	return 0;
}
