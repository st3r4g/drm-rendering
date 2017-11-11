//C standard
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

//POSIX
#include <unistd.h>

//logind
#include <systemd/sd-login.h>
#include <systemd/sd-bus.h>

int main() {
	int ret;
	char *id;
	unsigned int vt;
	char *seat;
	sd_bus *bus;

	ret = sd_pid_get_session(getpid(), &id);
	if (ret < 0) {
		fprintf(stderr, "Failed to get session id: %s\n", strerror(-ret));
		return -1;
	}
	fprintf(stdout, "Session id: %s\n", id);

	ret = sd_session_get_vt(id, &vt);
	if (ret < 0) {
		fprintf(stderr, "Session not running in vt: %s\n", strerror(-ret));
		return -1;
	}
	fprintf(stdout, "Virtual Terminal: %i\n", vt);
	
	ret = sd_session_get_seat(id, &seat);
	if (ret < 0) {
		fprintf(stderr, "Failed to get seat id: %s\n", strerror(-ret));
	}
	fprintf(stdout, "Seat id: %s\n", seat);

	ret = sd_bus_default_system(&bus);
	if (ret < 0) {
		fprintf(stderr, "Failed to open D-Bus connection: %s", strerror(-ret));
		return -1;
	}

	// Find session path
	
	free(seat);
	free(id);
	return 0;
}
