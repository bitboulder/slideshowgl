#ifndef _PANO_H
#define _PANO_H

struct ipano {
	char enable;
	float gw;
	float gh;
	float gyoff;
	float rotinit;
};

#include "img.h"
#include "load.h"

void panores(struct img *img,struct ipano *ip,int w,int h,int *xres,int *yres);
void panodrawimg(struct itx *tx,struct ipano *ip);
char panorender();
void panorun();

#endif
