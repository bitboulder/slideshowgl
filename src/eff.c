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
#include "help.h"
#include "file.h"

extern struct zoomtab {
	int move;
	float size;
	int inc;
	enum imgtex texcur;
	enum imgtex texoth;
} zoomtab[];

enum statmode { STAT_OFF, STAT_RISE, STAT_ON, STAT_FALL, STAT_NUM };

struct eval {
	float cur,dst;
	Uint32 tcur,tdst;
};

struct eff {
	enum effrefresh refresh;
	char ineff;
	struct wh maxfit;
	struct cfg {
		Uint32 efftime;
		float shrink;
		Uint32 stat_delay[STAT_NUM];
		Uint32 wnd_delay;
		float prged_w;
	} cfg;
	struct stat {
		enum statmode mode;
		Uint32 in,out;
		struct istat pos;
	} stat;
	struct eval ecat;
	struct {
		struct eval v;
		float *col;
		int actimgi;
	} eprgcol;
} eff = {
	.refresh = EFFREF_NO,
	.ineff = 0,
	.stat.mode = STAT_OFF,
	.stat.pos.h = 0.f,
	.ecat.cur = 0.f,
	.eprgcol.v.cur = 0.f,
	.eprgcol.col = NULL,
	.eprgcol.actimgi = -1,
};

#define AIL		(dp->actil&ACTIL)
#define AIMGI	(dp->imgi[AIL])

/* thread: all */
void effrefresh(enum effrefresh val){ eff.refresh|=val; }
char effineff(){ return eff.ineff; }
/* thread: gl */
struct istat *effstat(){ return &eff.stat.pos; }
float effcatf(){ return eff.ecat.cur; }
float effprgcolf(float **col){ *col=eff.eprgcol.col; return eff.eprgcol.v.cur; }

struct wh effmaxfit(){ return eff.maxfit; }

/***************************** imgpos *****************************************/

#define E(X)	#X,
const char *ipos_str[]={IPOS};
#undef E

#define E(X)	IPOS_##X,
enum eipos { IPOS NIPOS };
#undef E

#define E(X)	char FILL1_##X[sizeof(float)]; float X; char FILL2_##X[2*sizeof(Uint32)];
struct edst { IPOS };
#undef E
#define E(X)	char FILL1_##X[2*sizeof(float)]; Uint32 X; char FILL2_##X[sizeof(Uint32)];
struct etcur { IPOS };
#undef E
#define E(X)	char FILL1_##X[2*sizeof(float)+sizeof(Uint32)]; float X;
struct etdst { IPOS };
#undef E

#define E(X)	struct eval X;
union uipos {
	struct ecur cur;
	struct edst dst;
	struct etcur tcur;
	struct etdst tdst;
	struct eval a[NIPOS];
	struct { IPOS } e;
};
#undef E


struct imgpos {
	int eff;
	char* mark;
	char wayact;
	struct iopt opt;
	union uipos p;
	struct icol col;
};

/* thread: img */
struct imgpos *imgposinit(){ return calloc(1,sizeof(struct imgpos)); }
void imgposfree(struct imgpos *ip){ free(ip); }

/* thread: gl */
struct iopt *imgposopt(struct imgpos *ip){ return &ip->opt; }
struct ecur *imgposcur(struct imgpos *ip){ return &ip->p.cur; }
struct icol *imgposcol(struct imgpos *ip){ return &ip->col; }

/* thread: act */
char *imgposmark(struct img *img,enum mpcreate create){
	if(!img) return NULL;
	if(create==MPC_RESET) img->pos->mark=NULL;
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

void effposout(int imgi,struct imgpos *ip,Uint32 time){
	int i;
	for(i=0;i<NIPOS;i++){
		struct eval *e=ip->p.a;
		if(ip->eff && e->tdst)
			debug(DBG_EFF,"effpos img %3i (%6i) %4s: %5.2f->%5.2f (%4i->%4i)",imgi,time,ipos_str[i],e->cur,e->dst,e->tcur-time,e->tdst-time);
		else
			debug(DBG_EFF,"effpos img %3i (%6i) %4s: %5.2f",imgi,time,ipos_str[i],e->cur);
	}
}

void effsetnonpos(struct dplpos *dp,struct img *img,struct ecur *ip1,struct edst *ip2){
	float r=imgexifrotf(img->exif);
	float m=(img->pos->mark && img->pos->mark[0] && dp && dp->writemode)?1.f:0.f;
	if(ip1){ ip1->r+=r; ip1->m=m; }
	if(ip2){ ip2->r+=r; ip2->m=m; }
}

void effresetpos(struct dplpos *dp,struct img *img,struct ecur *ip){
	memset(ip,0,sizeof(struct edst));
	effsetnonpos(dp,img,ip,NULL);
}

void effinitpos(struct dplpos *dp,struct img *img,struct ecur *ip,int i){
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

void effsetcur(struct dplpos *dp,enum dplev ev,struct ecur *ip,int i){
	int diff=imgidiff(AIL,AIMGI,i,NULL,NULL);
	if(!diff || !(ev&DE_ZOOMOUT) || dp->zoom!=-1) return;
	ip->x = (float)(diff<0 ? 1-(1-diff)%3 : (diff+1)%3-1);
	ip->y = (float)((diff<0?diff-1:diff+1)/3);
	ip->a = 0.f;
	ip->s = 1.f;
}

char effsetdst(struct dplpos *dp,enum dplev ev,struct ecur *ip,int i){
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
	union uipos *ip=&img->pos->p;
	struct iopt *opt=&img->pos->opt;
	struct pev *pev=NULL;
	if(!prg) return 0;
	rev = AIMGI<dp->imgiold;
	if(!rev)       ievget=-1-ievget;
	if(ev&DE_JUMP) ievget=-1-ievget;
	num = prgget(prg,img,imginarrorlimits(AIL,AIMGI)+rev,ievget,&pev);
	if(!num || !pev){
		img->pos->eff=0;
		ip->cur.act=0;
	}else{
		int i;
		Uint32 time=SDL_GetTicks();
		Uint32 tstart=time;
		opt->tex=TEX_BIG;
		opt->layer=(char)pev->layer;
		for(i=0;i<5;i++){
			ip->a[i].cur=pev->way[(int) rev][i];
			ip->a[i].dst=pev->way[(int)!rev][i];
		}
		effsetnonpos(dp,img,&ip->cur,&ip->dst);
		ip->cur.act=1;
		ip->dst.act=(rev?pev->on:pev->off)?0.f:1.f;
		if(ev&(DE_JUMP|DE_INIT)){
			for(i=0;i<NIPOS;i++) ip->a[i].cur=ip->a[i].dst;
			img->pos->eff=0;
		}else if(!iev) img->pos->eff=num;
		if(!iev){
			float wt = rev ? 1.f-pev->waytime[1] : pev->waytime[0];
			tstart=time+(Uint32)((float)eff.cfg.efftime*wt);
		}
		for(i=0;i<NIPOS;i++)
			if(ip->a[i].cur==ip->a[i].dst) ip->a[i].tdst=0;
			else{
				if(!iev) ip->a[i].tcur=tstart;
				ip->a[i].tdst=ip->a[i].tcur+(Uint32)((float)eff.cfg.efftime*(pev->waytime[1]-pev->waytime[0]));
			}
		if(ip->cur.act) effposout((int)(((unsigned long)img)%1000),img->pos,time);
	}
	return 1;
}

void efffaston(struct dplpos *dp,union uipos *ip,int i){
	int i1,i2;
	int diff=imgidiff(AIL,dp->imgiold,AIMGI,&i1,&i2);
	if(!diff) return;
	if(diff<0 && (i<=i2 || i>=i1)) return;
	if(diff>0 && (i<=i1 || i>=i2)) return;
	ip->cur.act=1.f;
	ip->dst.act=1.f;
}

void effinittime(union uipos *ip,enum dplev ev){
	int i;
	Uint32 time=eff.cfg.efftime;
	if(ev&DE_INIT) time=0;
	for(i=0;i<NIPOS;i++) ip->a[i].tcur=time;
	if(ev&DE_JUMPX) ip->tcur.x=0;
	if(ev&DE_JUMPY) ip->tcur.y=0;
}

enum { EI_OFF=0x1, EI_CONSTSPEED=0x2 };
char effiniteval(struct eval *e,float dst,Uint32 tdst,unsigned int flags,Uint32 time){
	if(e->tdst){
		if(!tdst) e->cur+=dst-e->dst;
		else e->tdst=time + ((flags&EI_CONSTSPEED) ? (Uint32)(fabsf(e->cur-dst)*(float)tdst) : tdst);
		e->dst=dst;
	}else if(dst!=e->cur){
		if(!tdst || (flags&EI_OFF)) e->cur=dst;
		else{
			e->dst=dst;
			e->tdst=time+tdst;
			e->tcur=time;
		}
	}
	return e->tdst!=0;
}

void effinitval(struct imgpos *imgp,union uipos *ipn,int imgi){
	int i;
	union uipos *ipo=&imgp->p;
	Uint32 time=SDL_GetTicks();
	if(ipn->cur.act) ipo->cur.act=1.f;
	for(i=0;i<NIPOS;i++){
		struct eval *po=ipo->a+i;
		struct eval *pn=ipn->a+i;
		if(!ipo->cur.act) po->tdst=0;
		effiniteval(po,pn->cur,pn->tcur,ipo->cur.act?0:EI_OFF,time);
		if(po->tdst) imgp->eff=1;
		if(i==IPOS_r && po->tdst){
			while(po->cur-po->dst> 180.f) po->cur-=360.f;
			while(po->cur-po->dst<-180.f) po->cur+=360.f;
		}
	}
	if(ipo->cur.act) effposout(imgi,imgp,time);
}

void effinitimg(struct dplpos *dp,enum dplev ev,int i,int iev){
	struct img *img;
	union uipos *ipo;
	union uipos  ipn[1];
	char back=0;
	char neff=0;
	if(!(img=imgget(AIL,i))) return;
	ipo=&img->pos->p;
	img->pos->opt.tex=imgseltex(dp,i);
	if(dp->zoom==0 && effprg(dp,ev,img,iev)) return;
	effinitpos(dp,img,&ipn->cur,i);
	if(!ipn->cur.act && dp->zoom==0 && (ev&DE_VER) && dp->writemode) efffaston(dp,ipo,i);
	if(!ipn->cur.act && img->pos->eff==2) return;
	if( ipn->cur.act && !ipo->cur.act)      effsetcur(dp,ev,&ipo->cur,i);
	if(!ipn->cur.act &&  ipo->cur.act) neff=effsetdst(dp,ev,&ipn->cur,i);
	effinittime(ipn,iev?DE_INIT:ev);
	if(dp->zoom<0 && !(dp->zoom==-1 && (ev&DE_ZOOMOUT))){
		float xdiff=fabsf(ipo->cur.x-ipn->cur.x)/eff.maxfit.w/ipn->cur.s;
		float ydiff=fabsf(ipo->cur.y-ipn->cur.y)/eff.maxfit.h/ipn->cur.s;
		back = (xdiff>0.01f && ydiff>1.5f) || xdiff>1.5f || ydiff>3.5f;
	}
	ipn->cur.back=ipo->cur.back<.5f?0.f:1.f;
	if(back) ipn->cur.back=1.f-ipn->cur.back;
	img->pos->opt.layer=back;
	effinitval(img->pos,ipn,i);
	if(neff){
		if(img->pos->eff) img->pos->eff=2;
		else effinitpos(dp,img,&ipo->cur,i);
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
			effinitpos(dp,img,&img->pos->p.cur,i);
			img->pos->p.cur.a=0.f;
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
			if((img=imgget(AIL,i)) && img->pos->p.cur.act &&
					img->pos->p.cur.r != imgexifrotf(img->exif))
				effinitimg(dp,ev,i,0);
	}
}

void effdel(struct imgpos *imgp){
	union uipos *ipo=&imgp->p;
	union uipos  ipn[1];
	int i;
	if(!ipo->cur.act) return;
	for(i=0;i<NIPOS;i++) ipn->a[i].cur =
		imgp->eff && ipo->a[i].tdst ? ipo->a[i].dst : ipo->a[i].cur;
	ipn->cur.s=0.f;
	ipn->cur.r+=180.f;
	ipn->cur.act=0.f;
	effinittime(ipn,0);
	effinitval(imgp,ipn,-1);
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
	if(!panoend(&img->pos->p.cur.s)) return;
	img->pos->p.cur.s/=fith;
	while(img->pos->p.cur.x<-.5f) img->pos->p.cur.x+=1.f;
	while(img->pos->p.cur.x> .5f) img->pos->p.cur.x-=1.f;
	img->pos->p.cur.x*=img->pos->p.cur.s;
	img->pos->p.cur.y*=img->pos->p.cur.s;
}

char effcatinit(char dst){
	float edst;
	char ret;
	if(dst>=0) edst=dst?1.f:0.f;
	else edst=1.f-eff.ecat.dst;
	ret=eff.ecat.dst!=edst;
	effiniteval(&eff.ecat,edst,eff.cfg.wnd_delay,EI_CONSTSPEED,SDL_GetTicks());
	return ret;
}

int effprgcolinit(float *col,int actimgi){
	int actimgiold=eff.eprgcol.actimgi;
	if(actimgi==-2){
		struct img *img;
		struct txtimg *txt;
		if(eff.eprgcol.col && (img=imgget(1,eff.eprgcol.actimgi)) && (txt=imgfiletxt(img->file)))
			eff.eprgcol.col=txt->col;
		return -1;
	}
	if(col) eff.eprgcol.col=col;
	eff.eprgcol.actimgi=actimgi;
	effiniteval(&eff.eprgcol.v,actimgi>=0?1.f:0.f,eff.cfg.wnd_delay,EI_CONSTSPEED,SDL_GetTicks());
	return actimgiold;
}

void effprgcolset(int id){
	int b=id/NPRGCOL;
	int c=id%NPRGCOL;
	float chsl[4];
	if(eff.eprgcol.actimgi<=0) return;
	if(b<0 || b>=3) return;
	if(c<0 || c>=NPRGCOL) return;
	col_rgb2hsl(chsl,eff.eprgcol.col);
	chsl[b]=(float)c/((float)NPRGCOL-1.f);
	col_hsl2rgb(eff.eprgcol.col,chsl);
}

/***************************** eff do *****************************************/

float effcalclin(float a,float b,float ef){
	if(ef<=0.f) return a;
	if(ef>=1.f) return b;
	return (b-a)*ef+a;
}

char effdoeval(struct eval *e,Uint32 time){
	if(e->dst!=e->cur && time<e->tdst){
		if(time>e->tcur){
			e->cur+=(e->dst-e->cur)
				/(float)(e->tdst-e->tcur)
				*(float)(time-e->tcur);
			e->tcur=time;
		}
		return 1;
	}else if(e->tdst){
		e->cur=e->dst;
		e->tcur=e->tdst;
		e->tdst=0;
	}
	return 0;
}

void effdoimg(struct img *img,int imgi){
	union uipos *ip=&img->pos->p;
	Uint32 time=SDL_GetTicks();
	int i;
	char effon=0;
	if(!img->pos->eff) return;
	if(!ip->cur.act) return;
	for(i=0;i<NIPOS;i++) effon|=effdoeval(ip->a+i,time);
	if(!effon && --img->pos->eff)
		effinitimg(dplgetpos(),0,imgi,img->pos->eff);
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

void effdo(){
	struct img *img;
	char ineff=0;
	int il,i;
	Uint32 now;
	if(eff.refresh!=EFFREF_NO){
		effinit(eff.refresh,0,-1);
		eff.refresh=EFFREF_NO;
	}
	for(il=0;il<IL_NUM;il++)
		for(img=imgget(il,0),i=0;img;img=img->nxt,i++) if(img->pos->eff){
			effdoimg(img,i);
			ineff=1;
		}
	if(delimg){
		if(delimg->pos->eff) effdoimg(delimg,-1);
		if(!delimg->pos->eff){
			struct img *tmp=delimg;
			delimg=NULL;
			ldffree(tmp->ld,TEX_NONE);
		}
	}
	now=SDL_GetTicks();
	effdostat();
	effdoeval(&eff.ecat,now);
	effdoeval(&eff.eprgcol.v,now);
	eff.ineff=ineff;
}

void effcfginit(){
	eff.cfg.shrink=cfggetfloat("eff.shrink");
	eff.cfg.efftime=cfggetuint("eff.efftime");
	eff.cfg.stat_delay[STAT_OFF] = 0;
	eff.cfg.stat_delay[STAT_RISE]=cfggetuint("eff.stat_rise");
	eff.cfg.stat_delay[STAT_ON]  =cfggetuint("eff.stat_on");
	eff.cfg.stat_delay[STAT_FALL]=cfggetuint("eff.stat_fall");
	eff.cfg.wnd_delay=cfggetuint("eff.wnd_delay");
	eff.cfg.prged_w=cfggetfloat("prged.w");
}

unsigned int effdelay(int imgi,unsigned int dpldur){
	float on,stay;
	if(!prgdelay(imgi,&on,&stay)) return dpldur+eff.cfg.efftime;
	on*=(float)eff.cfg.efftime;
	stay*=(float)dpldur;
	return (unsigned int)(on+stay);
}
