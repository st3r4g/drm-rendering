#ifndef MYSESSION_H
#define MYSESSION_H

typedef struct _session session;

session *session_create();

int session_take_control(session *state);

int session_take_device(session *state, int major, int minor);

int session_release_device(session *state, int major, int minor);

int session_release_control(session *state);

int session_destroy(session *state);

#endif
