#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "prg.h"
#include "img.h"
#include "main.h"
#include "eff.h"
#include "dpl.h"
#include "help.h"

struct prg {
	int nfrm;
	struct prgev {
		struct prgev *nxt;
		struct img *img;
		float waytime[2];
		struct ipos way[2];
	} **ev;
};

int prggetn(struct prg *prg){ return prg->nfrm-1; }

void prgdestroy(struct prg *prg){
	int frm;
	for(frm=0;frm<prg->nfrm;frm++) while(prg->ev[frm]){
		struct prgev *ev=prg->ev[frm];
		prg->ev[frm]=ev->nxt;
		free(ev);
	}
	if(prg->ev) free(prg->ev);
	free(prg);
}

float *prgsplit(const char *txt){
	size_t len=strlen(txt);
	char *buf=malloc(len+1);
	char *pos,*tok;
	float *flt=NULL;
	size_t i=0,n=0;
	memcpy(buf,txt,len+1);
	pos=buf;
	while((tok=strsep(&pos,":"))){
		if(i==n) flt=realloc(flt,sizeof(float)*(n+=32));
		flt[i++]=(float)atof(tok);
	}
	if(flt && flt[0]*13+1==i) return flt;
	if(flt) free(flt);
	error(ERR_CONT,"split txt failed (%s)",txt);
	return NULL;
}

void prgevadd(struct prg *prg,float *flt,struct img *img){
	int frm=(int)flt[0];
	struct prgev *ev=calloc(1,sizeof(struct prgev));
	if(frm>=prg->nfrm){
		prg->ev=realloc(prg->ev,sizeof(struct ev *)*(size_t)(frm+1));
		while(frm>=prg->nfrm) prg->ev[prg->nfrm++]=NULL;
	}
	ev->img=img;
	memcpy(ev->waytime,flt+1,sizeof(float)*2);
	memcpy(ev->way+0,flt+3,sizeof(float)*5);
	memcpy(ev->way+1,flt+8,sizeof(float)*5);
	ev->nxt=prg->ev[frm];
	prg->ev[frm]=ev;
}

void prgadd(struct prg **prg,const char *txt,struct img *img){
	float *flt;
	int i;
	if(!(flt=prgsplit(txt))) return;
	if(!*prg) *prg=calloc(1,sizeof(struct prg));
	for(i=0;i<flt[0];i++) prgevadd(*prg,flt+1+i*13,img);
	free(flt);
}

int prgimgidiff(int frm,int imgi){
	struct prg *prg=ilprg();
	struct img *img=imgget(imgi);
	int fchg,fdir,f;
	struct prgev *ev;
	if(!prg || !img || dplgetzoom()!=0) return imgidiff(frm,imgi,NULL,NULL);
	/* TODO: ev-off no mark for imgi on in frame */
	for(fchg=0;fchg<prg->nfrm;fchg++) for(fdir=-1;fdir<=1;fdir+=2){
		f=frm+fchg*fdir;
		if(f<0 || f>=prg->nfrm) continue;
		for(ev=prg->ev[f];ev;ev=ev->nxt) if(ev->img==img) return fchg*fdir;
	}
	return imgi;
}

int prgget(struct prg *prg,struct img *img,int frm,char rev,int iev,struct ipos *way,float *waytime){
	struct prgev *ev;
	int num=0,i=0;
	if(frm<0 || frm>prg->nfrm) return 0;
	for(ev=prg->ev[frm];ev;ev=ev->nxt) if(img==ev->img) num++;
	if(!rev) iev=num-1-iev;
	for(ev=prg->ev[frm];ev;ev=ev->nxt) if(img==ev->img && iev==i++){
		way[(int) rev]=ev->way[0];
		way[(int)!rev]=ev->way[1];
		waytime[(int) rev]=rev ? 1.f-ev->waytime[0] : ev->waytime[0];
		waytime[(int)!rev]=rev ? 1.f-ev->waytime[1] : ev->waytime[1];
	}
	return num>iev ? num : 0;
}

char prgforoneldfrm(int frm,char (*func)(struct imgld *il,enum imgtex it),enum imgtex it){
	struct prg *prg=ilprg();
	if(!prg || dplgetzoom()!=0){
		struct img *img=imgget(frm);
		return img && func(img->ld,it);
	}else{
		struct prgev *ev;
		if(frm<0 || frm>=prg->nfrm) return 0;
		/* TODO: ev-off no mark for imgi on in frame */
		for(ev=prg->ev[frm];ev;ev=ev->nxt)
			if(func(ev->img->ld,it)) return 1;
		return 0;
	}
}