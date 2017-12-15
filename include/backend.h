#ifndef BACKEND_H
#define BACKEND_H

#include <gbm.h>

typedef struct _drm drm;

drm *drm_create();

int drm_get_fd(drm *state);
struct gbm_device *drm_get_gbm(drm *state);
struct gbm_surface *drm_get_surf(drm *state);

/*int drm_modeset(
	struct drm_info_t *drm_info,
	struct renderer_info_t *renderer_info,
	int fd
);*/

int drm_pageflip(drm *state);

int drm_handle_event(drm *state);

int drm_destroy(drm *state);


typedef struct _input input;

input *input_create();

int input_get_fd(input *state);

int input_handle_event(input *state);

int input_destroy(input *state);

#endif
