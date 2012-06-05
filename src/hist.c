#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "hist.h"
#include "help.h"
#include "cfg.h"

#define HDIV	(256/HDIM)

struct imghist {
	int num;
	float h[HT_NUM*HDIM];
};

struct histcfg {
	char init;
	int def[HT_NUM][2];
	float max;
} histcfg = {
	.init = 0,
	.def = { {1000,1000}, {0,2}, {1,1}, {2,0} },
};

struct imghist *imghistinit(){ return calloc(1,sizeof(struct imghist)); }
float *imghistget(struct imghist *ih){ return ih->h; }

void histinit(){
	if(histcfg.init) return;
	histcfg.max=cfggetfloat("hist.max")*(float)HDIV;
	histcfg.init=1;
}

unsigned char rgb2l(unsigned char *rgb){
	int M=MAX(rgb[0],MAX(rgb[1],rgb[2]));
	int m=MIN(rgb[0],MAX(rgb[1],rgb[2]));
	int r=(M+m)/2;
	if(r>255) r=255;
	if(r<0) r=0;
	return (unsigned char)r;
}

void imghistgen(struct imghist *ih,int num,char bgr,int bpp,unsigned char *pixels){
	int p,h,i;
	int hist[HT_NUM][HDIM];
	histinit();
	if(ih->num>=num) return;
	for(h=0;h<HT_NUM;h++) for(i=0;i<HDIM;i++) hist[h][i]=0;
	for(p=0;p<num;p++,pixels+=bpp) for(h=0;h<HT_NUM;h++){
		int hd=histcfg.def[h][bgr!=0];
		int val=hd>4 ? rgb2l(pixels) : pixels[hd];
		val/=HDIV;
		if(val>=HDIM) val=HDIM-1;
		if(val<0) val=0;
		hist[h][val]++;
	}
	for(h=0;h<HT_NUM;h++) for(i=0;i<HDIM;i++){
		float v=(float)hist[h][i]/(float)num/histcfg.max;
		if(v>1.f) v=1.f;
		ih->h[h*HDIM+i]=v;
	}
	ih->num=num;
}

void imghistclear(struct imghist *ih){ ih->num=0; }

