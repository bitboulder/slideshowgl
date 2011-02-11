#ifndef _DPL_INT_H
#define _DPL_INT_H

#include "dpl.h"

#define ACTIL			0x0000ffff
#define ACTIL_PRGED		0x00010000

struct dplpos {
	int imgi[IL_NUM];
	int imgiold;
	int zoom;
	float x,y;
	int actil;
	char writemode;
};

char imgfit(struct img *img,float *fitw,float *fith);

struct dplpos *dplgetpos();

#endif
