#ifndef _PANO_H
#define _PANO_H

#include "img.h"
#include "load.h"

//              winkeltreu  äquidistant flächentreu orthographisch
#define PANOFM	E2(ANGLE,0),E2(ADIST,1),E2(PLANE,2),E2(ORTHO,3)

struct imgpano *imgpanoinit();
void imgpanofree(struct imgpano *ip);
char imgpanoenable(struct imgpano *ip);
void imgpanoload(struct imgpano *ip,const char *fn);

void panoinit(char done);

struct img *panoactive();
char panostattxt(char *txt,size_t len);

void panores(struct imgpano *ip,int w,int h,int *xres,int *yres);
char panospos2ipos(struct img *img,float sx,float sy,float *ix,float *iy);
char panoclip(struct img *img,float *xb,float *yb);
void panotrimmovepos(struct img *img,float *ix,float *iy);

char panostart(struct img *img,float fitw,float *x);
char panoend(float *s);

void panodrawimg(struct itx *tx,struct imgpano *ip);
void panoperspective(float h3d,int fm,float w);
char panorender(char sel);

enum panoev { PE_PLAY, PE_SPEEDRIGHT, PE_SPEEDLEFT, PE_FLIPRIGHT, PE_FLIPLEFT, PE_MODE, PE_FISHMODE };
char panoev(enum panoev pe);
float panorot();

void panorun();

#endif
