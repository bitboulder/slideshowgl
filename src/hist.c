#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "hist.h"
#include "main.h"
#include "help.h"
#include "file.h"
#include "cfg.h"

#define HDIV	(256/HDIM)

struct imghist {
	char hfn[FILELEN];
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

void imghistload(struct imghist *ih,char *fn){
	FILE *fd;
	if(ih->hfn[0]) return;
	snprintf(ih->hfn,FILELEN,fn);
	switch(findfilesubdir(ih->hfn,"thumb",".hist",0)){
		case  0: ih->hfn[0]='\0';
		case -1: return;
	}
	if(filetime(ih->hfn)<filetime(fn)) return;
	debug(DBG_DBG,"imghistload file '%s'",ih->hfn);
	if(!(fd=fopen(ih->hfn,"rb"))){ error(ERR_CONT,"imghistload open file failed '%s'",ih->hfn); return; }
	if(fread(&ih->num,sizeof(int),1,fd)<1 || fread(ih->h,sizeof(float),HT_NUM*HDIM,fd)<HT_NUM*HDIM){
		ih->num=0; error(ERR_CONT,"imghistload file read failed '%s'",ih->hfn);
	}
	fclose(fd);
}

void imghistsave(struct imghist *ih){
	FILE *fd;
	if(!ih->hfn[0]) return;
	debug(DBG_DBG,"imghistsave file '%s'",ih->hfn);
	mkdirm(ih->hfn,1);
	if(!(fd=fopen(ih->hfn,"wb"))){ error(ERR_CONT,"imghistsave open file failed '%s'",ih->hfn); return; }
	if(fwrite(&ih->num,sizeof(int),1,fd)<1 || fwrite(ih->h,sizeof(float),HT_NUM*HDIM,fd)<HT_NUM*HDIM)
		error(ERR_CONT,"imghistload file write failed '%s'",ih->hfn);
	fclose(fd);
}

void imghistgen(struct imghist *ih,char *fn,int num,char bgr,int bpp,unsigned char *pixels){
	int p,h,i;
	int hist[HT_NUM][HDIM];
	histinit();
	imghistload(ih,fn);
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
	imghistsave(ih);
}

void imghistclear(struct imghist *ih){
	ih->hfn[0]='\0';
	ih->num=0;
}

