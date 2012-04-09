#ifndef _DPL_INT_H
#define _DPL_INT_H

#include "dpl.h"

enum ailtyp {AIL_NONE=0x0, AIL_PRGED=0x1, AIL_DIRED=0x2};
#define AIL_ED (AIL_PRGED|AIL_DIRED)

struct dplpos {
	int imgiold;
	int zoom;
	float x,y;
	enum ailtyp ailtyp;
	struct img *buble;
};

char dplwritemode();
Uint32 dplgetticks();

char imgfit(struct img *img,float *fitw,float *fith);

struct dplpos *dplgetpos();

#endif
