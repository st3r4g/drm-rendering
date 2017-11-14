#include <session.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <systemd/sd-login.h>

int init_session() {
	int ret;
	char *id = NULL;
	unsigned int vt;
	char *seat = NULL;

	ret = sd_pid_get_session(0, &id);
	if (ret < 0) {
		fprintf(stderr, "err: %s\n", strerror(-ret));
		goto end;
	}
	printf("session id: %s\n", id);

	ret = sd_session_get_vt(id, &vt);
	if (ret < 0) {
		fprintf(stderr, "err: %s\n", strerror(-ret));
		goto end;
	}
	printf("session vt: %i\n", vt);

	ret = sd_session_get_seat(id, &seat);
	if (ret < 0) {
		fprintf(stderr, "err: %s\n", strerror(-ret));
		goto end;
	}
	printf("session seat: %s\n", seat);

end:
	free(seat);
	free(id);

	return ret < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
