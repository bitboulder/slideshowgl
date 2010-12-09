#ifndef _PANO_H
#define _PANO_H

#include "img.h"
#include "load.h"

struct imgpano *imgpanoinit();
void imgpanofree(struct imgpano *ip);
char imgpanoenable(struct imgpano *ip);
void imgpanoload(struct imgpano *ip,char *fn);

void panoinit();

struct img *panoactive();

void panores(struct img *img,struct imgpano *ip,int w,int h,int *xres,int *yres);
char panospos2ipos(struct img *img,float sx,float sy,float *ix,float *iy);
char panoclipx(struct img *img,float *xb);

char panostart(float *x);
char panoend(float *s);

void panodrawimg(struct itx *tx,struct imgpano *ip);
char panorender();

char panoplay();
char panospeed(int dir);
void panoflip(int dir);
void panoplain();

void panorun();

#endif
