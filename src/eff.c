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

struct imgpos {
	int eff;
	char* mark;
	char wayact;
	struct iopt opt;
	struct ipos cur;
	struct ipos way[2];
	Uint32 waytime[2];
	struct icol col;
};

/* thread: img */
struct imgpos *imgposinit(){ return calloc(1,sizeof(struct imgpos)); }
void imgposfree(struct imgpos *ip){ free(ip); }

/* thread: gl */
struct iopt *imgposopt(struct imgpos *ip){ return &ip->opt; }
struct ipos *imgposcur(struct imgpos *ip){ return &ip->cur; }
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

void effwaytime(struct imgpos *ip,Uint32 len){
	ip->waytime[1]=(ip->waytime[0]=SDL_GetTicks())+len;
}

int effdiff(struct dplpos *dp,int *pi1,int *pi2){
	return imgidiff(AIL,dp->imgiold,AIMGI,pi1,pi2);
}

void effmove(struct dplpos *dp,struct ipos *ip,int i){
	struct img *img=imgget(AIL,i);
	ip->a = 1.f;
	ip->m=(img && img->pos->mark && img->pos->mark[0] && dp->writemode)?1.f:0.f;
	ip->r=img ? imgexifrotf(img->exif) : 0.f;
	if(dp->actil==ACTIL_PRGED){
		int diff=imgidiff(AIL,AIMGI,i,NULL,NULL);
		ip->s=(AIMGI==i ? 1.f : .75f) * eff.cfg.prged_w;
		ip->x=-0.4f;
		ip->y=(float)diff*eff.cfg.prged_w;
	}else if(dp->zoom<0){
		int diff=imgidiff(AIL,AIMGI,i,NULL,NULL);
		ip->s=zoomtab[-dp->zoom].size;
		ip->x=(float)diff*ip->s;
		ip->y=0.;
		while(ip->x<-.5f){ ip->x+=1.f; ip->y-=ip->s; }
		while(ip->x> .5f){ ip->x-=1.f; ip->y+=ip->s; }
		if(i!=AIMGI) ip->s*=eff.cfg.shrink;
		ip->x*=eff.maxfit.w;
		ip->y*=eff.maxfit.h;
	}else if(panoactive()){
		ip->s=powf(2.f,(float)dp->zoom);
		ip->x=-dp->x;
		ip->y=-dp->y;
	}else{
		float fitw,fith;
		ip->s=powf(2.f,(float)dp->zoom);
		ip->x=-dp->x*ip->s;
		ip->y=-dp->y*ip->s;
		if(imgfit(img,&fitw,&fith)){
			ip->x*=fitw;
			ip->y*=fith;
		}
	}
	/*printf("=> %.2f %.2f %.2f %.2f\n",ip->a,ip->s,ip->x,ip->y);*/
}

char effonoff(struct dplpos *dp, struct ipos *ip,struct ipos *ipon,enum dplev ev,char neg){
	if(neg) ev=DE_NEG(ev);
	if(dp->zoom>0 || (ev&DE_ZOOM)){
		memset(ip,0,sizeof(struct ipos));
		ip->m=ipon->m;
		ip->r=ipon->r;
		return 1;
	}
	*ip=*ipon;
	if(dp->actil==ACTIL_PRGED){
		ip->a = 1.f;
		ip->y += (float)(effdiff(dp,NULL,NULL) * (neg?-1:1))*eff.cfg.prged_w;
	}else if(dp->zoom==0){
		if(dp->writemode) ip->x += (float)(effdiff(dp,NULL,NULL) * (neg?-1:1));
		else ip->a=0.;
	}else if(AIMGI==IMGI_END || dp->imgiold==IMGI_END){
		ip->a=0.;
	}else{
		ip->a=0.;
		if(ev&(DE_RIGHT|DE_LEFT)) ip->x += (ip->x<0.?-1.f:1.f) * zoomtab[-dp->zoom].size;
		if(ev&(DE_UP|DE_DOWN|DE_SEL)) ip->y += (ip->y<0.?-1.f:1.f) * zoomtab[-dp->zoom].size;
	}
	return 0;
}

char efffaston(struct dplpos *dp,struct imgpos *ip,int i){
	int i1,i2;
	int diff=effdiff(dp,&i1,&i2);
	struct img *img=imgget(AIL,i);
	if(!diff) return 0;
	if(diff<0 && (i<=i2 || i>=i1)) return 0;
	if(diff>0 && (i<=i1 || i>=i2)) return 0;
	ip->opt.active=2;
	ip->cur.a=1.;
	ip->cur.s=1.;
	ip->cur.x=(float)(i-i1);
	ip->cur.y=0.;
	ip->cur.m=(img && img->pos->mark && img->pos->mark[0] && dp->writemode)?1.:0.;
	ip->cur.r=img ? imgexifrotf(img->exif) : 0.f;
	ip->eff=0;
	return 1;
}

enum imgtex imgseltex(struct dplpos *dp,struct imgpos *ip,int i){
	if(dp->zoom>0)  return TEX_FULL;
	if(AIMGI==i) return zoomtab[-dp->zoom].texcur;
	else if(ip->opt.active==2) return TEX_SMALL;
	else return zoomtab[-dp->zoom].texoth;
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
		ip->opt.active=0;
	}else{
		ip->opt.tex=TEX_BIG;
		ip->opt.back=(char)pev->layer;
		ip->way[(int) rev]=pev->way[0];
		ip->way[(int)!rev]=pev->way[1];
		if(iev) ip->waytime[0]=ip->waytime[1];
		else{
			float wt = rev ? 1.f-pev->waytime[1] : pev->waytime[0];
			ip->waytime[0]=SDL_GetTicks()+(Uint32)((float)eff.cfg.efftime*wt);
		}
		ip->waytime[1]=ip->waytime[0]+(Uint32)((float)eff.cfg.efftime*(pev->waytime[1]-pev->waytime[0]));
		if(ev&DE_JUMP){
			ip->cur=ip->way[1];
			ip->eff=0;
		}else{
			ip->cur=ip->way[0];
			if(!iev) ip->eff=num;
			ip->wayact=(rev?pev->on:pev->off)?0:1;
		}
		ip->opt.active=1;
	}
	return 1;
}

void effinitimg(struct dplpos *dp,enum dplev ev,int i){
	struct img *img;
	struct imgpos *ip;
	struct ipos dst;
	char act;
	if(!(img=imgget(AIL,i))) return;
	if(dp->zoom==0 && effprg(dp,ev,img,0)) return;
	ip=img->pos;
	act=effact(dp,i);
	if(!act && !ip->opt.active){
		if(dp->zoom!=0 || !(ev&DE_VER) || !dp->writemode) return;
		if(!efffaston(dp,ip,i)) return;
	}
	if(!act && ip->eff && !ip->wayact) return;
	ip->opt.tex=imgseltex(dp,ip,i);
	ip->opt.back=0;
	if(act) effmove(dp,&dst,i);
	else  ip->opt.back|=effonoff(dp,&dst,&ip->cur,ev,1);
	if((ev&DE_JUMP) && act){
		if(ip->opt.active && ip->eff){
			if(ev&DE_JUMPX){
				float chg=dst.x-ip->way[1].x;
				ip->cur.x+=chg;
				ip->way[0].x+=chg;
				ip->way[1].x+=chg;
			}
			if(ev&DE_JUMPY){
				float chg=dst.y-ip->way[1].y;
				ip->cur.y+=chg;
				ip->way[0].y+=chg;
				ip->way[1].y+=chg;
			}
		}else{
			if(ev&DE_JUMPX) ip->cur.x=dst.x;
			if(ev&DE_JUMPY) ip->cur.y=dst.y;
			ip->opt.active=1;
		}
		return;
	}
	ip->way[1]=dst;
	if(!ip->opt.active){
		ip->opt.back|=effonoff(dp,ip->way+0,ip->way+1,ev,0);
		ip->cur=ip->way[0];
	}else{
		float xdiff=fabsf(ip->cur.x-ip->way[1].x)/eff.maxfit.w/ip->way[1].s;
		float ydiff=fabsf(ip->cur.y-ip->way[1].y)/eff.maxfit.h/ip->way[1].s;
		ip->way[0]=ip->cur;
		if(act && dp->zoom<0 &&
			((xdiff>0.01f && ydiff>1.5f) || xdiff>1.5f || ydiff>3.5f)
			) ip->opt.back=1;
	}
	if(!memcmp(&ip->cur,ip->way+1,sizeof(struct ipos))){
		ip->opt.active=act;
		return;
	}
	if(memcmp(&ip->cur,ip->way+1,sizeof(struct ipos))){
		effwaytime(ip,eff.cfg.efftime);
		ip->wayact=act;
		ip->eff=1;
		ip->opt.active=1;
	}else{
		ip->eff=0;
		ip->opt.active=act;
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
		for(img=imgget(AIL,0);img;img=img->nxt){ img->pos->opt.active=0; img->pos->eff=0; }
	if(effref&(EFFREF_FIT|EFFREF_CLR))
		if(dp->zoom<0 && effmaxfitupdate(dp))
			effref|=EFFREF_ALL;
	if(effref&(EFFREF_ALL|EFFREF_CLR))
		for(i=0,img=imgget(AIL,0);img;img=img->nxt,i++) effinitimg(dp,ev,i);
	else{
		if(effref&EFFREF_IMG)
			effinitimg(dp,ev,imgi<0?AIMGI:imgi);
		if(effref&EFFREF_ROT) for(i=0,img=imgget(AIL,0);img;img=img->nxt,i++)
			if((img=imgget(AIL,i)) && img->pos->opt.active &&
					img->pos->cur.r != imgexifrotf(img->exif))
				effinitimg(dp,ev,i);
	}
}

void effdel(struct imgpos *ip){
	if(!ip->opt.active) return;
	ip->way[1]=ip->way[0]=ip->cur;
	ip->way[1].s=0.f;
	effwaytime(ip,eff.cfg.efftime);
	ip->wayact=0;
	ip->opt.active=1;
	ip->opt.back=1;
	ip->eff=1;
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
	if(!panoend(&img->pos->cur.s)) return;
	img->pos->cur.s/=fith;
	while(img->pos->cur.x<-.5f) img->pos->cur.x+=1.f;
	while(img->pos->cur.x> .5f) img->pos->cur.x-=1.f;
	img->pos->cur.x*=img->pos->cur.s;
	img->pos->cur.y*=img->pos->cur.s;
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

float effcalcrot(float a,float b,float ef){
	while(b-a> 180.f) b-=360.f;
	while(b-a<-180.f) b+=360.f;
	if(ef<=0.f) return a;
	if(ef>=1.f) return b;
	return (b-a)*ef+a;
}

#define ECS_TIME	0.2f
#define ECS_SIZE	0.3f
float effcalcshrink(float ef){
	if(ef<ECS_TIME)         return 1.f-ef/ECS_TIME*(1.f-ECS_SIZE);
	else if(ef<1.-ECS_TIME) return ECS_SIZE;
	else                    return 1.f-(1.f-ef)/ECS_TIME*(1.f-ECS_SIZE);
}

void effimg(struct img *img){
	struct imgpos *ip=img->pos;
	Uint32 time=SDL_GetTicks();
	if(time>=ip->waytime[1]){
		ip->eff--;
		if(ip->eff) effprg(dplgetpos(),0,img,ip->eff); else {
			ip->cur=ip->way[1];
			ip->opt.active=ip->wayact;
		}
	}else if(time>ip->waytime[0]){
		float ef = (float)(time-ip->waytime[0]) / (float)(ip->waytime[1]-ip->waytime[0]);
		if(ip->way[0].a!=ip->way[1].a) ip->cur.a=effcalclin(ip->way[0].a,ip->way[1].a,ef);
		if(ip->way[0].s!=ip->way[1].s) ip->cur.s=effcalclin(ip->way[0].s,ip->way[1].s,ef); else ip->cur.s=ip->way[0].s;
		if(ip->way[0].x!=ip->way[1].x) ip->cur.x=effcalclin(ip->way[0].x,ip->way[1].x,ef);
		if(ip->way[0].y!=ip->way[1].y) ip->cur.y=effcalclin(ip->way[0].y,ip->way[1].y,ef);
		if(ip->way[0].m!=ip->way[1].m) ip->cur.m=effcalclin(ip->way[0].m,ip->way[1].m,ef);
		if(ip->way[0].r!=ip->way[1].r) ip->cur.r=effcalcrot(ip->way[0].r,ip->way[1].r,ef);
		if(ip->opt.back) ip->cur.s*=effcalcshrink(ef);
	}
	/*printf("%i %i %i %g\n",ip->waytime[0],ip->waytime[1],time,ip->cur.a);*/
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
	int il;
	if(eff.refresh!=EFFREF_NO){
		effinit(eff.refresh,0,-1);
		eff.refresh=EFFREF_NO;
	}
	for(il=0;il<IL_NUM;il++)
		for(img=imgget(il,0);img;img=img->nxt) if(img->pos->eff){
			effimg(img);
			ineff=1;
		}
	if(delimg){
		if(delimg->pos->eff) effimg(delimg);
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
