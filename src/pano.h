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

void panoinit();

struct img *panoactive();
void panoplain();

void panores(struct img *img,struct ipano *ip,int w,int h,int *xres,int *yres);
char panospos2ipos(struct img *img,float sx,float sy,float *ix,float *iy);
char panoclipx(struct img *img);

char panostart(float *x);
char panoend(float *x);

void panodrawimg(struct itx *tx,struct ipano *ip);
char panorender();

char panoplay();
char panospeed(int dir);
void panoflip(int dir);
void panorun();

#endif
