#ifndef _DPL_H
#define _DPL_H

struct dpl {
	int img;
	float img_x,img_y;
	float zoom;
};
extern struct dpl dpl;

void *dplthread(void *arg);

#endif
