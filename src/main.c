#include <mysession.h>
#include <mydrm.h>
#include <myegl.h>

int main() {
	struct session_info_t *session_info = create_session_info();
	destroy_session_info(session_info);
	return 0;
}
