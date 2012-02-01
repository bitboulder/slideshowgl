#ifndef _DPL_INT_H
#define _DPL_INT_H

#include "dpl.h"

#define ACTIL			0x0000ffff
#define ACTIL_ED		(~ACTIL)
#define ACTIL_PRGED		0x00010000
#define ACTIL_DIRED		0x00020000

struct dplpos {
	int imgi[IL_NUM];
	int imgiold;
	int zoom;
	float x,y;
	int actil;
	void *dat;
	int buble;
};

char dplwritemode();
Uint32 dplgetticks();

char imgfit(struct img *img,float *fitw,float *fith);

struct dplpos *dplgetpos();

#endif
