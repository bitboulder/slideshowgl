#include <stdio.h>
#include <math.h>
#include <SDL.h>
#include "eff_int.h"
#include "dpl_int.h"
#include "exif.h"
#include "pano.h"
#include "main.h"
#include "cfg.h"
#include "prg.h"
#include "mark.h"

extern struct zoomtab {
	int move;
	float size;
	int inc;
	enum imgtex texcur;
	enum imgtex texoth;
} zoomtab[];

enum statmode { STAT_OFF, STAT_RISE, STAT_ON, STAT_FALL, STAT_NUM };

struct eff {
	enum effrefresh refresh;
	char ineff;
	struct wh maxfit;
	struct cfg {
		Uint32 efftime;
		float shrink;
		Uint32 stat_delay[STAT_NUM];
		Uint32 cat_delay;
		float prged_w;
	} cfg;
	struct stat {
		enum statmode mode;
		Uint32 in,out;
		struct istat pos;
	} stat;
	struct cat {
		char on;
		Uint32 reach;
		float f;
	} cat;
} eff = {
	.refresh = EFFREF_NO,
	.ineff = 0,
	.stat.mode = STAT_OFF,
	.stat.pos.h = 0.f,
	.cat.on = 0,
	.cat.f = 0.f,
};

#define AIL		(dp->actil&ACTIL)
#define AIMGI	(dp->imgi[AIL])

/* thread: all */
void effrefresh(enum effrefresh val){ eff.refresh|=val; }
char effineff(){ return eff.ineff; }
/* thread: gl */
struct istat *effstat(){ return &eff.stat.pos; }
float effcatf(){ return eff.cat.f; }

struct wh effmaxfit(){ return eff.maxfit; }

/***************************** imgpos *****************************************/

#define E(X)	#X
const char *ipos_str[]={IPOS};
#undef E

#define E(X)	IPOS_##X
enum eipos { IPOS, NIPOS };
#undef E

union uipos {
	struct ipos v;
	float a[NIPOS];
};


struct imgpos {
	int eff;
	char* mark;
	char wayact;
	struct iopt opt;
	union uipos cur;
	union uipos dst;
	Uint32 tcur[NIPOS];
	Uint32 tdst[NIPOS];
	struct icol col;
};

/* thread: img */
struct imgpos *imgposinit(){ return calloc(1,sizeof(struct imgpos)); }
void imgposfree(struct imgpos *ip){ free(ip); }

/* thread: gl */
struct iopt *imgposopt(struct imgpos *ip){ return &ip->opt; }
struct ipos *imgposcur(struct imgpos *ip){ return &ip->cur.v; }
struct icol *imgposcol(struct imgpos *ip){ return &ip->col; }

/* thread: act */
char *imgposmark(struct img *img,enum mpcreate create){
	if(!img) return NULL;
	if(create>=MPC_YES && !img->pos->mark) img->pos->mark=markimgget(img,create==MPC_ALLWAYS ? MKC_YES : MKC_NO);
	return img->pos->mark;
}

/***************************** eff init ***************************************/

char effact(struct dplpos *dp,int i){
	if(AIMGI==IMGI_START || AIMGI==IMGI_END) return 0;
	if(i==AIMGI) return 1;
	if(dp->actil==ACTIL_PRGED) return abs(imgidiff(AIL,AIMGI,i,NULL,NULL))<=2;
	if(dp->zoom>=0) return 0;
	if(abs(imgidiff(AIL,AIMGI,i,NULL,NULL))<=zoomtab[-dp->zoom].inc) return 1;
	return 0;
}

enum imgtex imgseltex(struct dplpos *dp,int i){
	if(dp->zoom>0)  return TEX_FULL;
	if(AIMGI==i) return zoomtab[-dp->zoom].texcur;
	else return zoomtab[-dp->zoom].texoth;
}

void effposout(int imgi,struct imgpos *imgp,Uint32 time){
	float *cur=imgp->cur.a;
	float *dst=imgp->dst.a;
	Uint32 *tdst=imgp->tdst;
	Uint32 *tcur=imgp->tcur;
	int i;
	for(i=0;i<NIPOS;i++)
		if(imgp->eff && tdst[i])
			debug(DBG_EFF,"effpos img %3i (%6i) %4s: %5.2f->%5.2f (%4i->%4i)",imgi,time,ipos_str[i],cur[i],dst[i],tcur[i]-time,tdst[i]-time);
		else
			debug(DBG_EFF,"effpos img %3i (%6i) %4s: %5.2f",imgi,time,ipos_str[i],cur[i]);
}

void effsetnonpos(struct dplpos *dp,struct img *img,struct ipos *ip){
	ip->r+=imgexifrotf(img->exif);
	ip->m=(img->pos->mark && img->pos->mark[0] && dp && dp->writemode)?1.f:0.f;
}

void effresetpos(struct dplpos *dp,struct img *img,struct ipos *ip){
	memset(ip,0,sizeof(struct ipos));
	effsetnonpos(dp,img,ip);
}

void effinitpos(struct dplpos *dp,struct img *img,struct ipos *ip,int i){
	int diff=imgidiff(AIL,AIMGI,i,NULL,NULL);
	effresetpos(dp,img,ip);
	ip->act=effact(dp,i) ? 1.f : 0.f;
	ip->a=1.f;
	if(dp->actil==ACTIL_PRGED){
		ip->s=(AIMGI==i ? 1.f : .75f) * eff.cfg.prged_w;
		ip->x=-.4f;
		ip->y=(float)diff*eff.cfg.prged_w;
	}else if(dp->zoom<0){
		ip->s=zoomtab[-dp->zoom].size;
		ip->x=(float)diff*ip->s;
		ip->y=0.;
		while(ip->x<-.5f){ ip->x+=1.f; ip->y-=ip->s; }
		while(ip->x> .5f){ ip->x-=1.f; ip->y+=ip->s; }
		if(i!=AIMGI) ip->s*=eff.cfg.shrink;
		ip->x*=eff.maxfit.w;
		ip->y*=eff.maxfit.h;
	}else if(panoactive() && !diff){
		ip->s=powf(2.f,(float)dp->zoom);
		ip->x=-dp->x;
		ip->y=-dp->y;
	}else if(!diff){
		float fitw,fith;
		ip->s=powf(2.f,(float)dp->zoom);
		ip->x=-dp->x*ip->s;
		ip->y=-dp->y*ip->s;
		if(imgfit(img,&fitw,&fith)){
			ip->x*=fitw;
			ip->y*=fith;
		}
	}else{
		ip->s=1.f;
		ip->x = dp->writemode ? (float)diff : 0.f;
		ip->y=0.f;
		ip->a = dp->writemode ? 1.f : 0.f;
	}
}

void effsetcur(struct dplpos *dp,enum dplev ev,struct ipos *ip,int i){
	int diff=imgidiff(AIL,AIMGI,i,NULL,NULL);
	if(!diff || !(ev&DE_ZOOMOUT) || dp->zoom!=-1) return;
	ip->x = (float)(diff<0 ? 1-(1-diff)%3 : (diff+1)%3-1);
	ip->y = (float)((diff<0?diff-1:diff+1)/3);
	ip->a = 0.f;
	ip->s = 1.f;
}

char effsetdst(struct dplpos *dp,enum dplev ev,struct ipos *ip,int i){
	int diff=imgidiff(AIL,AIMGI,i,NULL,NULL);
	if(!diff || !(ev&DE_ZOOMIN) || dp->zoom!=0) return 0;
	ip->x = (float)(diff<0 ? 1-(1-diff)%3 : (diff+1)%3-1);
	ip->y = (float)((diff<0?diff-1:diff+1)/3);
	ip->a = 0.f;
	ip->s = 1.f;
	return 1;
}

char effprg(struct dplpos *dp,enum dplev ev,struct img *img,int iev){
	struct prg *prg=ilprg(AIL);
	char rev;
	int ievget=iev;
	int num;
	struct imgpos *ip=img->pos;
	struct pev *pev=NULL;
	if(!prg) return 0;
	rev = AIMGI<dp->imgiold;
	if(!rev)       ievget=-1-ievget;
	if(ev&DE_JUMP) ievget=-1-ievget;
	num = prgget(prg,img,imginarrorlimits(AIL,AIMGI)+rev,ievget,&pev);
	if(!num || !pev){
		ip->eff=0;
		ip->cur.v.act=0;
	}else{
		int i;
		Uint32 time=SDL_GetTicks();
		Uint32 tstart=time;
		ip->opt.tex=TEX_BIG;
		ip->opt.layer=(char)pev->layer;
		ip->cur.v=pev->way[(int) rev];
		ip->dst.v=pev->way[(int)!rev];
		effsetnonpos(dp,img,&ip->cur.v);
		effsetnonpos(dp,img,&ip->dst.v);
		ip->cur.v.act=1;
		ip->dst.v.act=(rev?pev->on:pev->off)?0.f:1.f;
		if(ev&(DE_JUMP|DE_INIT)){
			ip->cur=ip->dst;
			ip->eff=0;
		}else if(!iev) ip->eff=num;
		if(!iev){
			float wt = rev ? 1.f-pev->waytime[1] : pev->waytime[0];
			tstart=time+(Uint32)((float)eff.cfg.efftime*wt);
		}
		for(i=0;i<NIPOS;i++)
			if(ip->cur.a[i]==ip->dst.a[i]) ip->tdst[i]=0;
			else{
				if(!iev) ip->tcur[i]=tstart;
				ip->tdst[i]=ip->tcur[i]+(Uint32)((float)eff.cfg.efftime*(pev->waytime[1]-pev->waytime[0]));
			}
		if(ip->cur.v.act) effposout((int)(((unsigned long)img)%1000),ip,time);
	}
	return 1;
}

void efffaston(struct dplpos *dp,struct img *img,int i){
	int i1,i2;
	int diff=imgidiff(AIL,dp->imgiold,AIMGI,&i1,&i2);
	if(!diff) return;
	if(diff<0 && (i<=i2 || i>=i1)) return;
	if(diff>0 && (i<=i1 || i>=i2)) return;
	img->pos->cur.v.act=1.f;
	img->pos->dst.v.act=1.f;
}

void effinittime(Uint32 *it,enum dplev ev){
	int i;
	Uint32 time=eff.cfg.efftime;
	if(ev&DE_INIT) time=0;
	for(i=0;i<NIPOS;i++) it[i]=time;
	if(ev&DE_JUMPX) it[IPOS_x]=0;
	if(ev&DE_JUMPY) it[IPOS_y]=0;
}

void effinitval(struct imgpos *imgp,union uipos ip,Uint32 *it,int imgi){
	int i;
	float *cur=imgp->cur.a;
	float *dst=imgp->dst.a;
	Uint32 *tdst=imgp->tdst;
	Uint32 *tcur=imgp->tcur;
	Uint32 time=SDL_GetTicks();
	if(ip.v.act) imgp->cur.v.act=1.f;
	for(i=0;i<NIPOS;i++){
		if(!imgp->cur.v.act) tdst[i]=0;
		if(tdst[i]){
			if(!it[i]) cur[i]+=ip.a[i]-dst[i];
			else tdst[i]=time+it[i];
			dst[i]=ip.a[i];
		}else if(ip.a[i]!=cur[i]){
			if(!it[i] || !imgp->cur.v.act) cur[i]=ip.a[i];
			else{
				dst[i]=ip.a[i];
				tdst[i]=time+it[i];
				tcur[i]=time;
			}
		}
		if(tdst[i]) imgp->eff=1;
		if(i==IPOS_r && tdst[i]){
			while(cur[i]-dst[i]> 180.f) cur[i]-=360.f;
			while(cur[i]-dst[i]<-180.f) cur[i]+=360.f;
		}
	}
	if(imgp->cur.v.act) effposout(imgi,imgp,time);
}

void effinitimg(struct dplpos *dp,enum dplev ev,int i,int iev){
	struct img *img;
	struct ipos *cur;
	struct ipos *dst;
	union uipos ip;
	Uint32 it[NIPOS];
	char back=0;
	char neff=0;
	if(!(img=imgget(AIL,i))) return;
	cur=&img->pos->cur.v;
	dst=&img->pos->dst.v;
	img->pos->opt.tex=imgseltex(dp,i);
	if(dp->zoom==0 && effprg(dp,ev,img,iev)) return;
	effinitpos(dp,img,&ip.v,i);
	if(!ip.v.act && dp->zoom==0 && (ev&DE_VER) && dp->writemode) efffaston(dp,img,i);
	if(!ip.v.act && img->pos->eff==2) return;
	if( ip.v.act && !cur->act) effsetcur(dp,ev,cur,i);
	if(!ip.v.act &&  cur->act) neff=effsetdst(dp,ev,&ip.v,i);
	effinittime(it,iev?DE_INIT:ev);
	if(dp->zoom<0 && !(dp->zoom==-1 && (ev&DE_ZOOMOUT))){
		float xdiff=fabsf(cur->x-ip.v.x)/eff.maxfit.w/ip.v.s;
		float ydiff=fabsf(cur->y-ip.v.y)/eff.maxfit.h/ip.v.s;
		back = (xdiff>0.01f && ydiff>1.5f) || xdiff>1.5f || ydiff>3.5f;
	}
	ip.v.back=cur->back<.5f?0.f:1.f;
	if(back) ip.v.back=1.f-ip.v.back;
	img->pos->opt.layer=back;
	effinitval(img->pos,ip,it,i);
	if(neff){
		if(img->pos->eff) img->pos->eff=2;
		else effinitpos(dp,img,cur,i);
	}
}

char effmaxfitupdate(struct dplpos *dp){
	int i;
	float maxfitw=0.f,maxfith=0.f;
	struct img *img;
	for(img=imgget(AIL,0),i=0;img;img=img->nxt,i++) if(effact(dp,i)){
		float fitw,fith;
		if(!imgfit(img,&fitw,&fith)) continue;
		if(fitw>maxfitw) maxfitw=fitw;
		if(fith>maxfith) maxfith=fith;
	}
	if(maxfitw==0.f) maxfitw=1.f;
	if(maxfith==0.f) maxfith=1.f;
	if(maxfitw==eff.maxfit.w && maxfith==eff.maxfit.h) return 0;
	eff.maxfit.w=maxfitw;
	eff.maxfit.h=maxfith;
	return 1;
}

void effinit(enum effrefresh effref,enum dplev ev,int imgi){
	int i;
	struct dplpos *dp=dplgetpos();
	struct img *img;
	debug(DBG_DBG,"eff refresh%s%s%s%s%s",
		effref&EFFREF_IMG?" (img)":"",
		effref&EFFREF_ALL?" (all)":"",
		effref&EFFREF_CLR?" (clr)":"",
		effref&EFFREF_FIT?" (fit)":"",
		effref&EFFREF_ROT?" (rot)":"");
	if(effref&EFFREF_CLR)
		for(img=imgget(AIL,0),i=0;img;img=img->nxt,i++){
			img->pos->eff=0;
			effinitpos(dp,img,&img->pos->cur.v,i);
			img->pos->cur.v.a=0.f;
		}
	if(effref&(EFFREF_FIT|EFFREF_CLR))
		if(dp->zoom<0 && effmaxfitupdate(dp))
			effref|=EFFREF_ALL;
	if(effref&(EFFREF_ALL|EFFREF_CLR))
		for(i=0,img=imgget(AIL,0);img;img=img->nxt,i++) effinitimg(dp,ev,i,0);
	else{
		if(effref&EFFREF_IMG)
			effinitimg(dp,ev,imgi<0?AIMGI:imgi,0);
		if(effref&EFFREF_ROT) for(i=0,img=imgget(AIL,0);img;img=img->nxt,i++)
			if((img=imgget(AIL,i)) && img->pos->cur.v.act &&
					img->pos->cur.v.r != imgexifrotf(img->exif))
				effinitimg(dp,ev,i,0);
	}
}

void effdel(struct imgpos *imgp){
	union uipos ip=imgp->eff ? imgp->dst : imgp->cur;
	Uint32 it[NIPOS];
	if(!imgp->cur.v.act) return;
	ip.v.s=0.f;
	ip.v.r+=180.f;
	ip.v.act=0.f;
	effinittime(it,0);
	effinitval(imgp,ip,it,-1);
	imgp->opt.layer=1;
}

void effstaton(){
	switch(eff.stat.mode){
		case STAT_OFF:
			eff.stat.mode=STAT_RISE;
			eff.stat.in=SDL_GetTicks();
			eff.stat.out=eff.stat.in+eff.cfg.stat_delay[STAT_RISE];
		break;
		case STAT_ON:
			eff.stat.out=SDL_GetTicks()+eff.cfg.stat_delay[STAT_ON];
		break;
		case STAT_FALL:
			eff.stat.mode=STAT_RISE;
			eff.stat.in=SDL_GetTicks();
			eff.stat.in-=(eff.stat.out-eff.stat.in)*eff.cfg.stat_delay[STAT_RISE]/eff.cfg.stat_delay[STAT_FALL];
			eff.stat.out=eff.stat.in+eff.cfg.stat_delay[STAT_RISE];
		break;
		default: break;
	}
}

void effpanoend(struct img *img){
	float fitw,fith;
	if(!img) return;
	if(!imgfit(img,&fitw,&fith)) return;
	if(!panoend(&img->pos->cur.v.s)) return;
	img->pos->cur.v.s/=fith;
	while(img->pos->cur.v.x<-.5f) img->pos->cur.v.x+=1.f;
	while(img->pos->cur.v.x> .5f) img->pos->cur.v.x-=1.f;
	img->pos->cur.v.x*=img->pos->cur.v.s;
	img->pos->cur.v.y*=img->pos->cur.v.s;
}

char effcatinit(char dst){
	float f;
	if(dst>=0 && eff.cat.on==dst) return 0;
	if(dst<0) dst=!eff.cat.on;
	eff.cat.on=dst;
	f = dst ? 1.f-eff.cat.f : eff.cat.f;
	eff.cat.reach=SDL_GetTicks()+(Uint32)((float)eff.cfg.cat_delay*f);
	return 1;
}

/***************************** eff do *****************************************/

float effcalclin(float a,float b,float ef){
	if(ef<=0.f) return a;
	if(ef>=1.f) return b;
	return (b-a)*ef+a;
}

void effimg(struct img *img,int imgi){
	struct imgpos *ip=img->pos;
	Uint32 time=SDL_GetTicks();
	int i;
	char effon=0;
	if(!ip->eff) return;
	if(!ip->cur.v.act) return;
	for(i=0;i<NIPOS;i++) if(ip->dst.a[i]!=ip->cur.a[i] && time<ip->tdst[i]){
		if(time>ip->tcur[i]){
			ip->cur.a[i]+=(ip->dst.a[i]-ip->cur.a[i])
				/(float)(ip->tdst[i]-ip->tcur[i])
				*(float)(time-ip->tcur[i]);
			ip->tcur[i]=time;
		}
		effon=1;
	}else if(ip->tdst[i]){
		ip->cur.a[i]=ip->dst.a[i];
		ip->tcur[i]=ip->tdst[i];
		ip->tdst[i]=0;
	}
	if(!effon && --ip->eff) effinitimg(dplgetpos(),0,imgi,ip->eff);
}

float effdostatef(){ return (float)(SDL_GetTicks()-eff.stat.in)/(float)(eff.stat.out-eff.stat.in); }

void effdostat(){
	float ef=effdostatef();
	if(eff.stat.mode!=STAT_OFF && ef>=1.f){
		if(eff.stat.mode!=STAT_ON || dplgetimgi(-1)!=IMGI_END){
			eff.stat.mode=(eff.stat.mode+1)%STAT_NUM;
			eff.stat.in=eff.stat.out;
			eff.stat.out=eff.stat.in+eff.cfg.stat_delay[eff.stat.mode];
			ef=effdostatef();
		}
	}
	switch(eff.stat.mode){
		case STAT_RISE: eff.stat.pos.h=effcalclin(0.f,1.f,ef); break;
		case STAT_FALL: eff.stat.pos.h=effcalclin(1.f,0.f,ef); break;
		case STAT_ON:   eff.stat.pos.h=1.f; break;
		default:        eff.stat.pos.h=0.f; break;
	}
}

void effdocat(){
	Uint32 now=SDL_GetTicks();
	if(eff.cat.reach<=now) eff.cat.f = eff.cat.on ? 1.f : 0.f;
	else{
		float f=(float)(eff.cat.reach-now)/(float)eff.cfg.cat_delay;
		eff.cat.f = eff.cat.on ? 1.f-f : f;
	}
}

void effdo(){
	struct img *img;
	char ineff=0;
	int il,i;
	if(eff.refresh!=EFFREF_NO){
		effinit(eff.refresh,0,-1);
		eff.refresh=EFFREF_NO;
	}
	for(il=0;il<IL_NUM;il++)
		for(img=imgget(il,0),i=0;img;img=img->nxt,i++) if(img->pos->eff){
			effimg(img,i);
			ineff=1;
		}
	if(delimg){
		if(delimg->pos->eff) effimg(delimg,-1);
		if(!delimg->pos->eff){
			struct img *tmp=delimg;
			delimg=NULL;
			ldffree(tmp->ld,TEX_NONE);
		}
	}
	effdostat();
	effdocat();
	eff.ineff=ineff;
}

void effcfginit(){
	eff.cfg.shrink=cfggetfloat("eff.shrink");
	eff.cfg.efftime=cfggetuint("eff.efftime");
	eff.cfg.stat_delay[STAT_OFF] = 0;
	eff.cfg.stat_delay[STAT_RISE]=cfggetuint("dpl.stat_rise");
	eff.cfg.stat_delay[STAT_ON]  =cfggetuint("dpl.stat_on");
	eff.cfg.stat_delay[STAT_FALL]=cfggetuint("dpl.stat_fall");
	eff.cfg.cat_delay=cfggetuint("dpl.cat_delay");
	eff.cfg.prged_w=cfggetfloat("prged.w");
}

unsigned int effdelay(int imgi,unsigned int dpldur){
	float on,stay;
	if(!prgdelay(imgi,&on,&stay)) return dpldur+eff.cfg.efftime;
	on*=(float)eff.cfg.efftime;
	stay*=(float)dpldur;
	return (unsigned int)(on+stay);
}
