#ifndef RENDERER_H
#define RENDERER_H

#include <gbm.h>

typedef struct _renderer renderer;

renderer *renderer_create(
	struct gbm_device *gbm,
	struct gbm_surface *surf
);

int renderer_render(renderer *state, int secs);

int renderer_destroy(renderer *state);

#endif
