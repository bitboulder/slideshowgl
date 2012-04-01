#ifndef _HIST_H
#define _HIST_H

#include "img.h"

#define HDIM	128

enum histtyp {HT_A,HT_R,HT_G,HT_B,HT_NUM};

struct imghist *imghistinit();
float *imghistget(struct imghist *ih);
void imghistgen(struct imghist *ih,int num,char bgr,int bpp,unsigned char *pixels);

#endif
