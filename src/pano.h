#ifndef _PANO_H
#define _PANO_H

#include "img.h"

struct ipano {
	char enable;
	float gw;
	float gh;
	float gyoff;
	float rotinit;
};

void panores(struct img *img,struct ipano *ip,int w,int h,int *xres,int *yres);
char panorender();
void panorun();

#endif
