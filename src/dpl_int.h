#ifndef _DPL_INT_H
#define _DPL_INT_H

#include "dpl.h"

struct dplpos {
	int imgi;
	int imgiold;
	int zoom;
	float x,y;
	char writemode;
};

char imgfit(struct img *img,float *fitw,float *fith);

struct dplpos *dplgetpos();

#endif
