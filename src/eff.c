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
#include "sdl.h"
#include "map_int.h"

extern struct zoomtab {
	int move;
	float size;
	int inc;
	enum imgtex texcur;
	enum imgtex texoth;
} zoomtab[];

enum statmode { STAT_OFF, STAT_RISE, STAT_ON, STAT_FALL, STAT_NUM };

enum evaltyp {E_LIN,E_MAPS,E_MAPXY};
struct eval {
	float cur,dst;
	Uint32 tcur,tdst;
	int typ;
};

struct eff {
	char ineff;
	struct wh maxfit;
	struct cfg {
		Uint32 efftime;
		float shrink;
		Uint32 stat_delay[STAT_NUM];
		Uint32 wnd_delay;
		Uint32 sw_delay[ESW_N];
		float prged_w;
	} cfg;
	struct stat {
		enum statmode mode;
		Uint32 in,out;
		struct istat pos;
	} stat;
	struct eval esw[ESW_N];
	struct {
		struct eval v;
		float *col;
		int actimgi;
	} eprgcol;
	Uint32 lastchg;
} eff;

#define AIL_LEFT	(ep->dp->ailtyp!=AIL_NONE && ilget(ep->il)==ilget(CIL(0)))

/* thread: all */
char effineff(){ return eff.ineff; }
/* thread: gl */
struct istat *effstat(){ return &eff.stat.pos; }
float effswf(enum esw id){ return id>=ESW_N ? 0.f : eff.esw[id].cur; }
float effprgcolf(float **col){ *col=eff.eprgcol.col; return eff.eprgcol.v.cur; }
Uint32 efflastchg(){ return eff.lastchg; }

struct wh effmaxfit(){ return eff.maxfit; }

/***************************** imgpos *****************************************/

#define E(X)	#X,
const char *ipos_str[]={IPOS};
#undef E

#define E(X)	IPOS_##X,
enum eipos { IPOS NIPOS };
#undef E

#define E(X)	char FILL1_##X[sizeof(float)]; float X; char FILL2_##X[2*sizeof(Uint32)+sizeof(int)];
struct edst { IPOS };
#undef E
#define E(X)	char FILL1_##X[2*sizeof(float)]; Uint32 X; char FILL2_##X[sizeof(Uint32)+sizeof(int)];
struct etcur { IPOS };
#undef E
#define E(X)	char FILL1_##X[2*sizeof(float)+sizeof(Uint32)]; Uint32 X; char FILL2_##X[sizeof(int)];
struct etdst { IPOS };
#undef E
#define E(X)	char FILL1_##X[2*sizeof(float)+2*sizeof(Uint32)]; int X;
struct etyp { IPOS };
#undef E

#define E(X)	struct eval X;
union uipos {
	struct ecur cur;
	struct edst dst;
	struct etcur tcur;
	struct etdst tdst;
	struct etyp typ;
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

struct effpar {
	struct dplpos *dp;
	struct imglist *il;
	int imgi;
	int cimgi;
	int diff;
	void *dat;
};

char effact(struct effpar *ep){
	if(ep->dat) return 1;
	if(ep->cimgi==IMGI_START || ep->cimgi==IMGI_END) return 0;
	if(ep->imgi==ep->cimgi) return 1;
	if(AIL_LEFT) return abs(ep->diff)<=2;
	if(ep->dp->zoom>=0) return 0;
	if(abs(ep->diff)<=zoomtab[-ep->dp->zoom].inc) return 1;
	return 0;
}

enum imgtex imgseltex(struct effpar *ep){
	if(ep->dp->zoom>0)  return TEX_FULL;
	if(ep->cimgi==ep->imgi) return zoomtab[-ep->dp->zoom].texcur;
	else return zoomtab[-ep->dp->zoom].texoth;
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

void effsetnonpos(struct img *img,struct ecur *ip1,struct edst *ip2){
	float r=imgexifrotf(img->exif);
	float m=(img->pos->mark && img->pos->mark[0] && dplwritemode())?1.f:0.f;
	if(ip1){ ip1->r+=r; ip1->m=m; }
	if(ip2){ ip2->r+=r; ip2->m=m; }
}

void effresetpos(struct img *img,struct ecur *ip){
	memset(ip,0,sizeof(struct edst));
	effsetnonpos(img,ip,NULL);
}

void effinitpos(struct effpar *ep,struct img *img,struct ecur *ip){
	effresetpos(img,ip);
	ip->act=effact(ep) ? 1.f : 0.f;
	ip->a=1.f;
	if(mapon() && ep->dat){
		ip->s=(float)((struct mappos *)ep->dat)->iz;
		ip->x=(float)((struct mappos *)ep->dat)->gx;
		ip->y=(float)((struct mappos *)ep->dat)->gy;
	}else if(AIL_LEFT){
		ip->s=(ep->cimgi==ep->imgi ? 1.f : .75f) * eff.cfg.prged_w;
		ip->x=-.4f;
		ip->y=(float)ep->diff*eff.cfg.prged_w;
	}else if(ep->dp->zoom<0){
		ip->s=zoomtab[-ep->dp->zoom].size;
		ip->x=(float)ep->diff*ip->s;
		ip->y=0.;
		while(ip->x<-.5f){ ip->x+=1.f; ip->y-=ip->s; }
		while(ip->x> .5f){ ip->x-=1.f; ip->y+=ip->s; }
		if(ep->imgi!=ep->cimgi) ip->s*=eff.cfg.shrink;
		ip->x*=eff.maxfit.w;
		ip->y*=eff.maxfit.h;
	}else if(panoactive() && !ep->diff){
		ip->s=powf(2.f,(float)ep->dp->zoom);
		ip->x=-ep->dp->x;
		ip->y=-ep->dp->y;
	}else if(!ep->diff){
		float fitw,fith;
		ip->s=dplgetz();
		ip->x=-ep->dp->x*ip->s;
		ip->y=-ep->dp->y*ip->s;
		if(imgfit(img,&fitw,&fith)){
			ip->x*=fitw;
			ip->y*=fith;
		}
	}else{
		ip->s=1.f;
		ip->x = dplwritemode()==1 ? (float)ep->diff : 0.f;
		ip->y=0.f;
		ip->a = dplwritemode()==1 ? 1.f : 0.f;
	}
}

void effsetcur(struct effpar *ep,enum dplev ev,struct ecur *ip){
	if(mapon() && ep->dat){
		ip->x=(float)((struct mappos *)ep->dat)->gx;
		ip->y=(float)((struct mappos *)ep->dat)->gy;
		ip->a=1.f;
		ip->s=3.f;
		return;
	}
	if(!ep->diff || !(ev&DE_ZOOMOUT) || ep->dp->zoom!=-1) return;
	ip->x = (float)(ep->diff<0 ? 1-(1-ep->diff)%3 : (ep->diff+1)%3-1);
	ip->y = (float)((ep->diff<0?ep->diff-1:ep->diff+1)/3);
	ip->a = 0.f;
	ip->s = 1.f;
}

char effsetdst(struct effpar *ep,enum dplev ev,struct ecur *ip){
	if(!ep->diff || !(ev&DE_ZOOMIN) || ep->dp->zoom!=0) return 0;
	ip->x = (float)(ep->diff<0 ? 1-(1-ep->diff)%3 : (ep->diff+1)%3-1);
	ip->y = (float)((ep->diff<0?ep->diff-1:ep->diff+1)/3);
	ip->a = 0.f;
	ip->s = 1.f;
	return 1;
}

char effprg(struct effpar *ep,enum dplev ev,struct img *img,int iev){
	struct prg *prg=ilprg(ep->il);
	char rev;
	int ievget=iev;
	int num;
	union uipos *ip=&img->pos->p;
	struct iopt *opt=&img->pos->opt;
	struct pev *pev=NULL;
	if(!prg) return 0;
	rev = ep->cimgi<ep->dp->imgiold;
	if(!rev)       ievget=-1-ievget;
	if(ev&DE_JUMP) ievget=-1-ievget;
	num = prgget(prg,img,ilrelimgi(ep->il,ep->cimgi)+rev,ievget,&pev);
	if(!num || !pev){
		img->pos->eff=0;
		ip->cur.act=0;
	}else{
		int i;
		Uint32 time=dplgetticks();
		Uint32 tstart=time;
		opt->tex=TEX_BIG;
		opt->layer=(char)pev->layer;
		for(i=0;i<5;i++){
			ip->a[i].cur=pev->way[(int) rev][i];
			ip->a[i].dst=pev->way[(int)!rev][i];
		}
		effsetnonpos(img,&ip->cur,&ip->dst);
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

void efffaston(struct effpar *ep,union uipos *ip){
	int i1=ilrelimgi(ep->il,ep->dp->imgiold);
	int i2=ilrelimgi(ep->il,ep->cimgi);
	if(!ep->diff) return;
	if(ep->diff<0 && (ep->imgi<=i2 || ep->imgi>=i1)) return;
	if(ep->diff>0 && (ep->imgi<=i1 || ep->imgi>=i2)) return;
	ip->cur.act=1.f;
	ip->dst.act=1.f;
}

void effinittime(union uipos *ip,enum dplev ev,float timefac){
	int i;
	Uint32 time=(Uint32)((float)eff.cfg.efftime*timefac);
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
	e->typ=E_LIN;
	return e->tdst!=0;
}

void effinitval(struct imgpos *imgp,union uipos *ipn,int imgi){
	int i;
	union uipos *ipo=&imgp->p;
	Uint32 time=dplgetticks();
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

void effinitimg(struct effpar *ep,struct img *img,enum dplev ev,int iev){
	union uipos *ipo;
	union uipos  ipn[1];
	char back=0;
	char neff=0;
	float timefac=1.f;
	ep->diff=ildiffimgi(ep->il,ep->cimgi,ep->imgi);
	ipo=&img->pos->p;
	if(!strncmp(imgfilefn(img->file),"[MAP]",6)) ep->dat=mapgetpos();
	img->pos->opt.tex=imgseltex(ep);
	if(ep->dp->zoom==0 && effprg(ep,ev,img,iev)) return;
	effinitpos(ep,img,&ipn->cur);
	if(!ipn->cur.act && ep->dp->zoom==0 && (ev&DE_VER) && dplwritemode()==1) efffaston(ep,ipo);
	if(!ipn->cur.act && img->pos->eff==2) return;
	if( ipn->cur.act && !ipo->cur.act)      effsetcur(ep,ev,&ipo->cur);
	if(!ipn->cur.act &&  ipo->cur.act) neff=effsetdst(ep,ev,&ipn->cur);
	if( ipn->cur.act && ep->dp->buble==img){ ipn->cur.a=ipo->cur.a==1.f?.5f:1.f; neff=1; iev=0; timefac=.5f; }
	if(mapon() && ep->dat && ipn->cur.s!=ipo->cur.s && (ev&DE_ZOOM)) timefac=.5f*fabsf(ipn->cur.s-ipo->cur.s);
	effinittime(ipn,iev?DE_INIT:ev,timefac);
	if(ep->dp->zoom<0 && !(ep->dp->zoom==-1 && (ev&DE_ZOOMOUT))){
		float xdiff=fabsf(ipo->cur.x-ipn->cur.x)/eff.maxfit.w/ipn->cur.s;
		float ydiff=fabsf(ipo->cur.y-ipn->cur.y)/eff.maxfit.h/ipn->cur.s;
		back = (xdiff>0.01f && ydiff>1.5f) || xdiff>1.5f || ydiff>3.5f;
	}
	ipn->cur.back=ipo->cur.back<.5f?0.f:1.f;
	if(back) ipn->cur.back=1.f-ipn->cur.back;
	img->pos->opt.layer=back;
	effinitval(img->pos,ipn,ep->imgi);
	if(neff){
		if(img->pos->eff) img->pos->eff=2;
		else effinitpos(ep,img,&ipo->cur);
	}
	if(mapon() && ep->dat){
		img->pos->p.typ.s=E_MAPS;
		img->pos->p.typ.x=E_MAPXY;
		img->pos->p.typ.y=E_MAPXY;
	}
}

char effmaxfitupdate(struct effpar *ep){
	float maxfitw=0.f,maxfith=0.f;
	struct img *img;
	for(img=ilimg(ep->il,0),ep->imgi=0;img;img=img->nxt,ep->imgi++) if(effact(ep)){
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

void effinit(enum effrefresh effref,enum dplev ev,struct imglist *il,int imgi){
	struct effpar ep[1]={ { .dp=dplgetpos(), .il=il, .cimgi=ilcimgi(il), .dat=NULL } };
	struct img *img;
	debug(DBG_DBG,"eff refresh%s%s%s%s%s",
		effref&EFFREF_IMG?" (img)":"",
		effref&EFFREF_ALL?" (all)":"",
		effref&EFFREF_CLR?" (clr)":"",
		effref&EFFREF_FIT?" (fit)":"",
		effref&EFFREF_ROT?" (rot)":"");
	if(effref&EFFREF_CLR)
		for(img=ilimg(il,0),ep->imgi=0;img;img=img->nxt,ep->imgi++){
			img->pos->eff=0;
			ep->diff=ildiffimgi(ep->il,ep->cimgi,ep->imgi);
			effinitpos(ep,img,&img->pos->p.cur);
			img->pos->p.cur.a=0.f;
		}
	if(effref&(EFFREF_FIT|EFFREF_CLR))
		if(ep->dp->zoom<0 && effmaxfitupdate(ep))
			effref|=EFFREF_ALL;
	if(effref&(EFFREF_ALL|EFFREF_CLR))
		for(ep->imgi=0,img=ilimg(il,0);img;img=img->nxt,ep->imgi++) effinitimg(ep,img,ev,0);
	else{
		if(effref&EFFREF_IMG){
			ep->imgi=imgi<0?(mapon()?0:ep->cimgi):imgi;
			if((img=ilimg(il,ep->imgi))) effinitimg(ep,img,ev,0);
		}
		if(effref&EFFREF_ROT) for(ep->imgi=0,img=ilimg(il,0);img;img=img->nxt,ep->imgi++)
			if(img->pos->p.cur.r != imgexifrotf(img->exif)){
				if(img->pos->p.cur.act) effinitimg(ep,img,ev,0);
				else img->pos->p.cur.r=imgexifrotf(img->exif);
			}
	}
	if(ev&(DE_JUMP|DE_INIT)) sdlforceredraw();
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
	effinittime(ipn,0,1.f);
	effinitval(imgp,ipn,-1);
	imgp->opt.layer=1;
}

void effstaton(){
	switch(eff.stat.mode){
		case STAT_OFF:
			eff.stat.mode=STAT_RISE;
			eff.stat.in=dplgetticks();
			eff.stat.out=eff.stat.in+eff.cfg.stat_delay[STAT_RISE];
		break;
		case STAT_ON:
			eff.stat.out=dplgetticks()+eff.cfg.stat_delay[STAT_ON];
		break;
		case STAT_FALL:
			eff.stat.mode=STAT_RISE;
			eff.stat.in=dplgetticks();
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

char effsw(enum esw id,char dst){
	float edst;
	char ret;
	if(id>=ESW_N) return 0;
	if(dst>=0) edst=dst?1.f:0.f;
	else edst=1.f-eff.esw[id].dst;
	ret=eff.esw[id].dst!=edst;
	effiniteval(eff.esw+id,edst,eff.cfg.sw_delay[id],EI_CONSTSPEED,dplgetticks());
	return ret;
}

int effprgcolinit(float *col,int actimgi){
	int actimgiold=eff.eprgcol.actimgi;
	if(actimgi==-2){
		struct img *img;
		struct txtimg *txt;
		if(eff.eprgcol.col && (img=ilimg(CIL(1),eff.eprgcol.actimgi)) && (txt=imgfiletxt(img->file)))
			eff.eprgcol.col=txt->col;
		return -1;
	}
	if(col) eff.eprgcol.col=col;
	eff.eprgcol.actimgi=actimgi;
	effiniteval(&eff.eprgcol.v,actimgi>=0?1.f:0.f,eff.cfg.wnd_delay,EI_CONSTSPEED,dplgetticks());
	return actimgiold;
}

void effprgcolset(int id){
	int b=(id-1)/NPRGCOL;
	int c=(id-1)%NPRGCOL;
	float chsl[4];
	if(id<0) return;
	if(eff.eprgcol.actimgi<=0) return;
	if(b<0 || b>=3) return;
	if(c<0 || c>=NPRGCOL) return;
	col_rgb2hsl(chsl,eff.eprgcol.col);
	chsl[b]=((float)c+0.5f)/(float)NPRGCOL;
	col_hsl2rgb(eff.eprgcol.col,chsl);
}

/***************************** eff do *****************************************/

float effcalclin(float a,float b,float ef){
	if(ef<=0.f) return a;
	if(ef>=1.f) return b;
	return (b-a)*ef+a;
}

char effdoeval(struct eval *e,Uint32 time){
	static const float tfacmod=.5f;
	if(e->dst!=e->cur && time<e->tdst){
		if(time>e->tcur){
			float tfac=(float)(time-e->tcur)/(float)(e->tdst-e->tcur);
			switch(e->typ){
			case E_LIN:
				e->cur+=(e->dst-e->cur)*tfac;
			break;
			case E_MAPXY:
				e->cur+=(e->dst-e->cur)*powf(tfac,tfacmod);
			break;
			case E_MAPS:
				e->cur-=logf(1-(1-powf(2.f,e->cur-e->dst))*powf(tfac,tfacmod))/logf(2.f);
			break;
			}
			e->tcur=time;
		}
		return 1;
	}else if(e->tdst){
		e->cur=e->dst;
		e->tcur=e->tdst;
		e->tdst=0;
		return 1;
	}
	return 0;
}

char effdoimg(struct img *img,int imgi){
	union uipos *ip=&img->pos->p;
	Uint32 time=dplgetticks();
	int i;
	char effon=0;
	char chg=0;
	if(!img->pos->eff) return 0;
	if(!ip->cur.act) return 0;
	for(i=0;i<NIPOS;i++){
		if(effdoeval(ip->a+i,time)) chg=1;
		if(ip->a[i].tdst) effon=1;
	}
	if(!effon && --img->pos->eff){
		struct effpar ep[1]={ { .dp=dplgetpos(), .il=NULL/*TODO*/, .imgi=imgi, .cimgi=ilcimgi(ep->il), .dat=NULL } };
		effinitimg(ep,img,0,img->pos->eff);
	}
	return chg;
}

float effdostatef(){ return (float)(dplgetticks()-eff.stat.in)/(float)(eff.stat.out-eff.stat.in); }

char effdostat(){
	float ef=effdostatef();
	float h;
	if(eff.stat.mode!=STAT_OFF && ef>=1.f){
		if(eff.stat.mode!=STAT_ON || ilcimgi(NULL)!=IMGI_END){
			eff.stat.mode=(eff.stat.mode+1)%STAT_NUM;
			eff.stat.in=eff.stat.out;
			eff.stat.out=eff.stat.in+eff.cfg.stat_delay[eff.stat.mode];
			ef=effdostatef();
		}
	}
	switch(eff.stat.mode){
		case STAT_RISE: h=effcalclin(0.f,1.f,ef); break;
		case STAT_FALL: h=effcalclin(1.f,0.f,ef); break;
		case STAT_ON:   h=1.f; break;
		default:        h=0.f; break;
	}
	if(h==eff.stat.pos.h) return 0;
	eff.stat.pos.h=h;
	return 1;
}

struct effdoarg { char ineff; char chg; };

int effdo1(struct img *img,int imgi,void *arg){
	struct effdoarg *ea=(struct effdoarg*)arg;
	enum effrefresh effref=ilgeteffref(img->il);
	if(effref!=EFFREF_NO) effinit(effref,0,img->il,-1);
	if(!img->pos->eff) return 0;
	if(effdoimg(img,imgi)) ea->chg=1;
	ea->ineff=1;
	return 0;
}

void effdo(){
	int i;
	Uint32 now;
	struct effdoarg ea[1]={ { .ineff=0, .chg=0 } };
	ilsforallimgs(effdo1,ea,1,0);
	if(delimg){
		if(delimg->pos->eff) if(effdoimg(delimg,-1)) ea->chg=1;
		if(!delimg->pos->eff){
			struct img *tmp=delimg;
			delimg=NULL;
			ldffree(tmp->ld,TEX_NONE);
		}
	}
	now=dplgetticks();
	if(effdostat()) ea->chg=1;
	for(i=0;i<ESW_N;i++) if(effdoeval(eff.esw+i,now)) ea->chg=1;
	if(effdoeval(&eff.eprgcol.v,now)) ea->chg=1;
	eff.ineff=ea->ineff;
	if(ea->chg) eff.lastchg=SDL_GetTicks();
}

void effcfginit(){
	memset(&eff,0,sizeof(struct eff));
	eff.stat.mode = STAT_OFF,
	eff.eprgcol.actimgi = -1,
	eff.cfg.shrink=cfggetfloat("eff.shrink");
	eff.cfg.efftime=cfggetuint("eff.efftime");
	eff.cfg.stat_delay[STAT_OFF] = 0;
	eff.cfg.stat_delay[STAT_RISE]=cfggetuint("eff.stat_rise");
	eff.cfg.stat_delay[STAT_ON]  =cfggetuint("eff.stat_on");
	eff.cfg.stat_delay[STAT_FALL]=cfggetuint("eff.stat_fall");
	eff.cfg.wnd_delay=cfggetuint("eff.wnd_delay");
	eff.cfg.sw_delay[ESW_CAT]=eff.cfg.wnd_delay;
	eff.cfg.sw_delay[ESW_INFO]=cfggetuint("eff.txt_delay");
	eff.cfg.sw_delay[ESW_INFOSEL]=cfggetuint("eff.txt_delay");
	eff.cfg.sw_delay[ESW_HELP]=cfggetuint("eff.txt_delay");
	eff.cfg.sw_delay[ESW_HIST]=cfggetuint("eff.txt_delay");
	eff.cfg.prged_w=cfggetfloat("prged.w");
}

unsigned int effdelay(int imgi,unsigned int dpldur){
	float on,stay;
	if(!prgdelay(imgi,&on,&stay)) return dpldur+eff.cfg.efftime;
	on*=(float)eff.cfg.efftime;
	stay*=(float)dpldur;
	return (unsigned int)(on+stay);
}
