#include <stdlib.h>
#include "config.h"
#include "dpl.h"
#include "sdl.h"

struct dpl dpl = {
	.img = -1,
	.img_x = 0.5,
	.img_y = 0.5,
	.zoom = 1.,
};

void *dplthread(void *arg){
	while(!sdl.quit){
		SLEEP(10);
	}
	return NULL;
}
