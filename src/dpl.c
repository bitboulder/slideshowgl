#include <stdlib.h>
#include <SDL.h>
#include <math.h>
#include "dpl.h"
#include "sdl.h"
#include "load.h"
#include "cfg.h"
#include "main.h"
#include "exif.h"
#include "act.h"

struct dplpos {
	int imgi;
	int imgiold;
	int zoom;
	float x,y;
};

enum statmode { STAT_OFF, STAT_RISE, STAT_ON, STAT_FALL, STAT_NUM };
enum colmode { COL_NONE=-1, COL_G=0, COL_B=1, COL_C=2 };

char *colmodestr[]={"G","B","C"};

struct dpl {
	struct dplpos pos;
	float maxfitw,maxfith;
	Uint32 run;
	enum effrefresh refresh;
	char ineff;
	char showinfo;
	char showhelp;
	int inputnum;
	char writemode;
	struct {
		Uint32 efftime;
		Uint32 displayduration;
		float shrink;
		char loop;
		Uint32 stat_delay[STAT_NUM];
	} cfg;
	struct stat {
		enum statmode mode;
		Uint32 in,out;
		struct istat pos;
	} stat;
	enum colmode colmode;
} dpl = {
	.pos.imgi = -1,
	.pos.zoom = 0,
	.pos.x = 0.,
	.pos.y = 0.,
	.run = 0,
	.ineff = 0,
	.showinfo = 0,
	.showhelp = 0,
	.inputnum = 0,
	.writemode = 0,
	.refresh = EFFREF_NO,
	.stat.mode = STAT_OFF,
	.stat.pos.h = 0.f,
	.colmode = COL_NONE,
};

/***************************** dpl interface **********************************/

/* thread: all */
void effrefresh(enum effrefresh val){ dpl.refresh|=val; }
int dplgetimgi(){ return dpl.pos.imgi; }
int dplgetzoom(){ return dpl.pos.zoom; }
char dplineff(){ return dpl.ineff; }
char dplshowinfo(){ return dpl.showinfo; }
int dplinputnum(){ return dpl.inputnum; }
char dplloop(){ return dpl.cfg.loop; }
struct istat *dplstat(){ return &dpl.stat.pos; }

/***************************** dpl imgpos *************************************/

struct imgpos {
	char eff;
	char mark;
	struct iopt opt;
	struct ipos cur;
	struct ipos way[2];
	Uint32 waytime[2];
	char   wayact;
	struct icol col;
};

/* thread: img */
struct imgpos *imgposinit(){ return calloc(1,sizeof(struct imgpos)); }
void imgposfree(struct imgpos *ip){ free(ip); }

/* thread: gl */
struct iopt *imgposopt(struct imgpos *ip){ return &ip->opt; }
struct ipos *imgposcur(struct imgpos *ip){ return &ip->cur; }
struct icol *imgposcol(struct imgpos *ip){ return &ip->col; }

/* thread: load */
void imgpossetmark(struct imgpos *ip){ ip->mark=1; }
char imgposmark(struct imgpos *ip){ return ip->mark; }

char imgfit(struct img *img,float *fitw,float *fith){
	float irat;
	float srat=sdlrat();
	enum rot rot;
	if(!img || !(irat=imgldrat(img->ld))) return 0;
	rot=imgexifrot(img->exif);
	if(rot==ROT_90 || rot==ROT_270) irat=1./irat;
	*fitw = srat>irat ? irat/srat : 1.;
	*fith = srat>irat ? 1. : srat/irat;
	return 1;
}

char imgspos2ipos(struct img *img,float sx,float sy,float *ix,float *iy){
	float fitw,fith;
	if(dpl.pos.zoom<0) return 0;
	if(panospos2ipos(img,sx,sy,ix,iy)) return 1;
	if(!imgfit(img,&fitw,&fith)) return 0;
	fitw*=powf(2.f,(float)dpl.pos.zoom);
	fith*=powf(2.f,(float)dpl.pos.zoom);
	*ix = sx/fitw;
	*iy = sy/fith;
	return 1;
}

void printixy(float sx,float sy){
	struct img *img=imgget(dpl.pos.imgi);
	float ix,iy;
	if(!img || dpl.pos.zoom<0) return;
	if(!(imgspos2ipos(img,sx,sy,&ix,&iy))) return;
	debug(DBG_NONE,"img pos %.3fx%.3f",ix+dpl.pos.x,iy+dpl.pos.y);
}

/***************************** dpl img move ***********************************/

struct zoomtab {
	int move;
	float size;
	int inc;
	enum imgtex texcur;
	enum imgtex texoth;
} zoomtab[]={
	{ .move=5, .size=1.,    .inc=0,  .texcur=TEX_BIG,   .texoth=TEX_BIG,   },
	{ .move=3, .size=1./3., .inc=4,  .texcur=TEX_SMALL, .texoth=TEX_SMALL, },
	{ .move=5, .size=1./5., .inc=12, .texcur=TEX_SMALL, .texoth=TEX_TINY,  },
	{ .move=7, .size=1./7., .inc=24, .texcur=TEX_TINY,  .texoth=TEX_TINY,  },
};

char effact(int i){
	if(dpl.pos.imgi<0 || dpl.pos.imgi>=nimg) return 0;
	if(i==dpl.pos.imgi) return 1;
	return dpl.pos.zoom<0 && abs(i-dpl.pos.imgi)<=zoomtab[-dpl.pos.zoom].inc;
}

void effwaytime(struct imgpos *ip,Uint32 len){
	ip->waytime[1]=(ip->waytime[0]=SDL_GetTicks())+len;
}

void effmove(struct ipos *ip,int i){
	ip->a = 1.f;
	ip->m=(imgs[i]->pos->mark && dpl.writemode)?1.f:0.f;
	ip->r=imgexifrotf(imgs[i]->exif);
	if(dpl.pos.zoom<0){
		ip->s=zoomtab[-dpl.pos.zoom].size;
		ip->x=(i-dpl.pos.imgi)*ip->s;
		ip->y=0.;
		while(ip->x<-.5f){ ip->x+=1.f; ip->y-=ip->s; }
		while(ip->x> .5f){ ip->x-=1.f; ip->y+=ip->s; }
		if(i!=dpl.pos.imgi) ip->s*=dpl.cfg.shrink;
		ip->x*=dpl.maxfitw;
		ip->y*=dpl.maxfith;
	}else if(panoactive()){
		ip->s=powf(2.f,(float)dpl.pos.zoom);
		ip->x=-dpl.pos.x;
		ip->y=-dpl.pos.y;
	}else{
		float fitw,fith;
		ip->s=powf(2.f,(float)dpl.pos.zoom);
		ip->x=-dpl.pos.x*ip->s;
		ip->y=-dpl.pos.y*ip->s;
		if(imgfit(imgs[i],&fitw,&fith)){
			ip->x*=fitw;
			ip->y*=fith;
		}
	}
	/*printf("=> %.2f %.2f %.2f %.2f\n",ip->a,ip->s,ip->x,ip->y);*/
}

char effonoff(struct ipos *ip,struct ipos *ipon,enum dplev ev){
	if(dpl.pos.zoom>0 || (ev&DE_ZOOM)){
		memset(ip,0,sizeof(struct ipos));
		ip->m=ipon->m;
		ip->r=ipon->r;
		return 1;
	}
	*ip=*ipon;
	if(dpl.pos.imgi>=nimg || dpl.pos.imgiold>=nimg){
		ip->a=0.;
	}else if(dpl.pos.zoom==0){
		if(dpl.writemode) switch((int)ev){
			case DE_RIGHT: ip->x += 1.; break;
			case DE_LEFT:  ip->x -= 1.; break;
			case DE_UP:    ip->x += zoomtab[0].move; break;
			case DE_DOWN:  ip->x -= zoomtab[0].move; break;
		}else ip->a=0.;
	}else{
		ip->a=0.;
		switch((int)ev){
		case DE_RIGHT:
		case DE_LEFT:  ip->x += (ip->x<0.?-1.:1.) * zoomtab[-dpl.pos.zoom].size; break;
		case DE_SEL:
		case DE_UP:
		case DE_DOWN:  ip->y += (ip->y<0.?-1.:1.) * zoomtab[-dpl.pos.zoom].size; break;
		}
	}
	return 0;
}

char efffaston(struct imgpos *ip,enum dplev ev,int i){
	int diff=dpl.pos.imgi-i;
	if((ev!=DE_DOWN || diff>=0 || diff<=-zoomtab[0].move) &&
	   (ev!=DE_UP   || diff<=0 || diff>= zoomtab[0].move)) return 0;
	ip->opt.active=2;
	ip->cur.a=1.;
	ip->cur.s=1.;
	ip->cur.x=((float)DE_DIR(ev) * zoomtab[0].move) - (float)diff;
	ip->cur.y=0.;
	ip->cur.m=(imgs[i]->pos->mark && dpl.writemode)?1.:0.;
	ip->cur.r=imgexifrotf(imgs[i]->exif);
	ip->eff=0;
	return 1;
}

enum imgtex imgseltex(struct imgpos *ip,int i){
	if(dpl.pos.zoom>0)  return TEX_FULL;
	if(dpl.pos.imgi==i) return zoomtab[-dpl.pos.zoom].texcur;
	else if(ip->opt.active==2) return TEX_SMALL;
	else return zoomtab[-dpl.pos.zoom].texoth;
}

void effinitimg(enum dplev ev,int i){
	struct imgpos *ip;
	char act;
	if(i<0 || i>=nimg) return;
	ip=imgs[i]->pos;
	act=effact(i);
	if(!act && !ip->opt.active){
		if(dpl.pos.zoom!=0 || (ev&DE_VER) || !dpl.writemode) return;
		if(!efffaston(ip,ev,i)) return;
	}
	if(!act && ip->eff && !ip->wayact) return;
	/*printf("%i %3s\n",i,act?"on":"off");*/
	ip->opt.tex=imgseltex(ip,i);
	ip->opt.back=0;
	if(act) effmove(ip->way+1,i);
	else  ip->opt.back|=effonoff(ip->way+1,&ip->cur,DE_NEG(ev));
	if(!ip->opt.active){
		ip->opt.back|=effonoff(ip->way+0,ip->way+1,ev);
		ip->cur=ip->way[0];
	}else{
		ip->way[0]=ip->cur;
		if(act && dpl.pos.zoom<0 &&
			(fabs(ip->way[0].x-ip->way[1].x)>.5f ||
			 fabs(ip->way[0].y-ip->way[1].y)>.5f)
			) ip->opt.back=1;
	}
	if(!memcmp(&ip->cur,ip->way+1,sizeof(struct ipos))){
		ip->opt.active=act;
		return;
	}
	if(ev==DE_MOVE && act){
		ip->cur.x=ip->way[1].x;
		ip->cur.y=ip->way[1].y;
		ip->opt.active=1;
	}else if(memcmp(&ip->cur,ip->way+1,sizeof(struct ipos))){
		effwaytime(ip,dpl.cfg.efftime);
		ip->wayact=act;
		ip->eff=1;
		ip->opt.active=1;
	}else{
		ip->eff=0;
		ip->opt.active=act;
	}
}

char effmaxfit(){
	int i;
	float maxfitw=0.f,maxfith=0.f;
	for(i=0;i<nimg;i++) if(effact(i)){
		float fitw,fith;
		if(!imgfit(imgs[i],&fitw,&fith)) continue;
		if(fitw>maxfitw) maxfitw=fitw;
		if(fith>maxfith) maxfith=fith;
	}
	if(maxfitw==0.f) maxfitw=1.f;
	if(maxfith==0.f) maxfith=1.f;
	if(maxfitw==dpl.maxfitw && maxfith==dpl.maxfith) return 0;
	dpl.maxfitw=maxfitw;
	dpl.maxfith=maxfith;
	return 1;
}

void effinit(enum effrefresh effref,enum dplev ev){
	int i;
	debug(DBG_DBG,"eff refresh%s%s",
		effref&EFFREF_IMG?" (img)":"",
		effref&EFFREF_ALL?" (all)":"",
		effref&EFFREF_FIT?" (fit)":"");
	if(effref&EFFREF_FIT) if(dpl.pos.zoom<0 && effmaxfit()) effref|=EFFREF_ALL;
	if(effref&EFFREF_ALL) for(i=0;i<nimg;i++) effinitimg(ev,i);
	else if(effref&EFFREF_IMG) effinitimg(ev,dpl.pos.imgi);
	
}

void dplclippos(struct img *img){
	float xb,yb;
	float x[2],y[2];
	char clipx;
	if(!imgspos2ipos(img,.5f,.5f,&xb,&yb)) return;
	clipx=panoclipx(img,&xb);
	y[0]=-.5f+yb; y[1]= .5f-yb;
	if(y[1]<y[0]) y[0]=y[1]=0.f;
	if(dpl.pos.y<y[0]) dpl.pos.y=y[0];
	if(dpl.pos.y>y[1]) dpl.pos.y=y[1];
	if(!clipx) return;
	x[0]=-.5f+xb; x[1]= .5f-xb;
	if(x[1]<x[0]) x[0]=x[1]=0.f;
	if(dpl.pos.x<x[0]){ dpl.pos.x=x[0]; panoflip(-1); }
	if(dpl.pos.x>x[1]){ dpl.pos.x=x[1]; panoflip( 1); }
}

void dplmovepos(float sx,float sy){
	float ix,iy;
	struct img *img=imgget(dpl.pos.imgi);
	if(!img || !imgspos2ipos(img,sx,sy,&ix,&iy)) return;
	dpl.pos.x+=ix;
	dpl.pos.y+=iy;
	dplclippos(img);
	debug(DBG_DBG,"dpl move pos => imgi %i zoom %i pos %.2fx%.2f",dpl.pos.imgi,dpl.pos.zoom,dpl.pos.x,dpl.pos.y);
}

void dplzoompos(int nzoom,float sx,float sy){
	float oix,oiy,nix,niy;
	char o,n;
	struct img *img=imgget(dpl.pos.imgi);
	o=imgspos2ipos(img,sx,sy,&oix,&oiy);
	dpl.pos.zoom=nzoom;
	n=imgspos2ipos(img,sx,sy,&nix,&niy);
	if(o && n){
		dpl.pos.x+=oix-nix;
		dpl.pos.y+=oiy-niy;
	}
	if(img) dplclippos(img);
}

void dplclipimgi(){
	if(dpl.cfg.loop){
		while(dpl.pos.imgi<0)     dpl.pos.imgi+=nimg;
		while(dpl.pos.imgi>=nimg) dpl.pos.imgi-=nimg;
	}else{
		if(dpl.pos.imgi<0)    dpl.pos.imgi=0;
		if(dpl.pos.imgi>nimg) dpl.pos.imgi=nimg;
	}
}

void dplmove(enum dplev ev,float x,float y){
	const static int zoommin=sizeof(zoomtab)/sizeof(struct zoomtab);
	int dir=DE_DIR(ev);
	dpl.pos.imgiold=dpl.pos.imgi;
	switch(ev){
	case DE_RIGHT:
	case DE_LEFT:
		if(!panospeed(dir)){
			if(dpl.pos.zoom<=0) dpl.pos.imgi+=dir;
			else dplmovepos((float)dir*.25f,0.f);
		}
	break;
	case DE_UP:
	case DE_DOWN:
		if(dpl.pos.zoom<0)  dpl.pos.imgi-=dir*zoomtab[-dpl.pos.zoom].move;
		if(dpl.pos.zoom==0) dpl.pos.imgi+=dir*zoomtab[-dpl.pos.zoom].move;
		else dplmovepos(0.f,-(float)dir*.25f);
	break;
	case DE_ZOOMIN:
	case DE_ZOOMOUT:
	{
		float x;
		if(dpl.pos.zoom==0 && dir>0 && panostart(&x)){
			struct img *img=imgget(dpl.pos.imgi);
			dpl.pos.x=x;
			dpl.pos.zoom+=dir;
			dplclippos(img);
		}else if(dpl.pos.zoom+dir<=0){
			if(dpl.pos.zoom==1 && panoactive()){
				struct img *img=imgget(dpl.pos.imgi);
				float fitw,fith;
				img->pos->cur.x*=2.f;
				img->pos->cur.y*=2.f;
				if(imgfit(img,&fitw,&fith)){
					img->pos->cur.x/=fitw;
					img->pos->cur.y/=fith;
				}
			}
			dpl.pos.x=dpl.pos.y=0.;
			dpl.pos.zoom+=dir;
		}else dplzoompos(dpl.pos.zoom+dir,x,y);
	}
	break;
	default: return;
	}
	if(dpl.pos.zoom<1-zoommin) dpl.pos.zoom=1-zoommin;
	dplclipimgi();
	if(dpl.pos.zoom>0)    dpl.run=0;
	debug(DBG_STA,"dpl move => imgi %i zoom %i pos %.2fx%.2f",dpl.pos.imgi,dpl.pos.zoom,dpl.pos.x,dpl.pos.y);
	effinit(EFFREF_ALL|EFFREF_FIT,ev);
}

void dplsel(float sx,float sy){
	int i,x,y;
	if(dpl.pos.zoom>=0) return;
	sx/=dpl.maxfitw; if(sx> .49f) sx= .49f; if(sx<-.49f) sx=-.49f;
	sy/=dpl.maxfith; if(sy> .49f) sy= .49f; if(sy<-.49f) sy=-.49f;
	x=floor(sx/zoomtab[-dpl.pos.zoom].size+.5f);
	y=floor(sy/zoomtab[-dpl.pos.zoom].size+.5f);
	i=floor((float)y/zoomtab[-dpl.pos.zoom].size+.5f);
	i+=x;
	dpl.pos.imgi+=i;
	dplclipimgi();
	effinit(EFFREF_ALL|EFFREF_FIT,DE_SEL);
}

void dplmoveabs(int imgi){
	if(imgi<0 || imgi>nimg) return;
	dpl.pos.imgi=imgi;
	effinit(EFFREF_ALL|EFFREF_FIT,0);
}

void dplmark(){
	struct img *img=imgget(dpl.pos.imgi);
	if(!img) return;
	img->pos->mark=!img->pos->mark;
	effinit(EFFREF_IMG,0);
	actadd(ACT_SAVEMARKS,NULL);
}

void dplrotate(enum dplev ev){
	struct img *img=imgget(dpl.pos.imgi);
	int r=DE_DIR(ev);
	if(!img) return;
	exifrotate(img->exif,r);
	effinit(EFFREF_IMG|EFFREF_FIT,0);
	if(dpl.writemode) actadd(ACT_ROTATE,img);
}

void effdel(struct imgpos *ip){
	ip->way[1]=ip->way[0]=ip->cur;
	ip->way[1].s=0.f;
	effwaytime(ip,dpl.cfg.efftime);
	ip->wayact=0;
	ip->opt.active=1;
	ip->opt.back=1;
	ip->eff=1;
}

void dpldel(){
	struct img *img=imgdel(dpl.pos.imgi);
	if(!img) return;
	if(dpl.pos.zoom>0) dpl.pos.zoom=0;
	if(delimg){
		struct img *tmp=delimg;
		delimg=NULL;
		ldffree(tmp->ld,TEX_NONE);
	}
	if(!img->pos->opt.active) return;
	effdel(img->pos);
	delimg=img;
	effinit(EFFREF_ALL|EFFREF_FIT,1);
	if(dpl.writemode) actadd(ACT_DELETE,img);
}

#define ADDTXT(...)	txt+=snprintf(txt,dpl.stat.pos.txt+ISTAT_TXTSIZE-txt,__VA_ARGS__)
void dplstaton(char on){
	if(dpl.pos.imgi<0) snprintf(dpl.stat.pos.txt,ISTAT_TXTSIZE,"ANFANG");
	else if(dpl.pos.imgi>=nimg) snprintf(dpl.stat.pos.txt,ISTAT_TXTSIZE,"ENDE");
	else{
		struct img *img=imgs[dpl.pos.imgi];
		char *txt=dpl.stat.pos.txt;
		ADDTXT("%i/%i %s",dpl.pos.imgi+1, nimg, imgldfn(img->ld));
		switch(imgexifrot(img->exif)){
			case ROT_0: break;
			case ROT_90:  ADDTXT(" rotated-right"); break;
			case ROT_180: ADDTXT(" rotated-twice"); break;
			case ROT_270: ADDTXT(" rotated-left"); break;
		}
		if(dpl.writemode){
			ADDTXT(" (write-mode)");
			if(img->pos->mark) ADDTXT(" [MARK]");
		}
		if(dpl.colmode!=COL_NONE || img->pos->col.b || img->pos->col.b || img->pos->col.c){
			ADDTXT(" G:%.1f",img->pos->col.g);
			ADDTXT(" B:%.1f",img->pos->col.b);
			ADDTXT(" C:%.1f",img->pos->col.c);
		}
		if(dpl.colmode!=COL_NONE) ADDTXT(" [%s]",colmodestr[dpl.colmode]);
	}
	if(!on) return;
	switch(dpl.stat.mode){
		case STAT_OFF:
			dpl.stat.mode=STAT_RISE;
			dpl.stat.in=SDL_GetTicks();
			dpl.stat.out=dpl.stat.in+dpl.cfg.stat_delay[STAT_RISE];
		break;
		case STAT_ON:
			dpl.stat.out=SDL_GetTicks()+dpl.cfg.stat_delay[STAT_ON];
		break;
		case STAT_FALL:
			dpl.stat.mode=STAT_RISE;
			dpl.stat.in=SDL_GetTicks();
			dpl.stat.in-=(dpl.stat.out-dpl.stat.in)*dpl.cfg.stat_delay[STAT_RISE]/dpl.cfg.stat_delay[STAT_FALL];
			dpl.stat.out=dpl.stat.in+dpl.cfg.stat_delay[STAT_RISE];
		break;
		default: break;
	}
}

void dplcol(int d){
	struct img *img;
	float *val;
	if(dpl.colmode==COL_NONE) return;
	if(!(img=imgget(dpl.pos.imgi))) return;
	val=((float*)&img->pos->col)+dpl.colmode;
	*val+=.1f*(float)d;
	if(*val<-1.f) *val=-1.f;
	if(*val> 1.f) *val= 1.f;
	//if(dpl.colmode==COL_G) imgldrefresh(img->ld); /* TODO: check */
}

/***************************** dpl action *************************************/

void dplsetdisplayduration(int dur){
	if(dur<100) dur*=1000;
	if(dur>=250 && dur<=30000) dpl.cfg.displayduration=dur;
}

/***************************** dpl key ****************************************/

#define DPLEVS_NUM	128
struct dplevs {
	struct ev {
		enum dplev ev;
		SDLKey key;
		float sx,sy;
	} evs[DPLEVS_NUM];
	struct {
		float sx,sy;
	} move;
	int wi,ri;
} dev = {
	.wi = 0,
	.ri = 0,
	.move.sx = 0.f,
};

/* thread: sdl */
void dplevputx(enum dplev ev,SDLKey key,float sx,float sy){
	if(ev==DE_MOVE){
		dev.move.sy+=sy;
		dev.move.sx+=sx;
	}else{
		int nwi=(dev.wi+1)%DPLEVS_NUM;
		dev.evs[dev.wi].ev=ev;
		dev.evs[dev.wi].key=key;
		dev.evs[dev.wi].sx=sx;
		dev.evs[dev.wi].sy=sy;
		if(nwi!=dev.ri) dev.wi=nwi;
	}
}

char *keyboardlayout=
	"space\0"       "stop/play\0"
	"right\0"       "forward (zoom: shift right)\0"
	"left\0"        "backward (zoom: shift left)\0"
	"up\0"          "fast forward (zoom: shift up)\0"
	"down\0"        "fast backward (zoom: shift down)\0"
	"page-up\0"     "zoom in\0"
	"page-down\0"   "zoom out\0"
	"[0-9]+enter\0" "goto image with number\0"
	"[0-9]+d\0"     "displayduration [s/ms]\0"
	"f\0"           "switch fullscreen\0"
	"r/R\0"         "rotate image\0"
	"w\0"           "switch writing mode\0"
	"g/b/c\0"       "+/- mode: gamma/brightness/contrase\0"
	"+/-\0"         "increase/decrease selected\0"
	"p\0"           "panorama\0"
	"Del\0"         "move image to del/ and remove from dpl-list (only in writing mode)\0"
	"m\0"           "toggle mark (only in writing mode)\0"
	"i\0"           "show image info\0"
	"h\0"           "show help\0"
	"q/esc\0"       "quit\0"
	"\0"
;

char *dplhelp(){
	return dpl.showhelp ? keyboardlayout : NULL;
}

void dplkey(SDLKey key){
	debug(DBG_STA,"dpl key %i",key);
	if(key!=SDLK_PLUS && key!=SDLK_MINUS)
		dpl.colmode=COL_NONE;
	switch(key){
	case SDLK_ESCAPE:   if(dpl.inputnum || dpl.showinfo || dpl.showhelp) break;
	case SDLK_BACKSPACE:panoplain(); break;
	case SDLK_q:        sdl_quit=1; break;
	case SDLK_f:        sdlfullscreen(); break;
	case SDLK_w:        dpl.writemode=!dpl.writemode; effrefresh(EFFREF_ALL); break;
	case SDLK_m:        if(dpl.writemode) dplmark(); break;
	case SDLK_d:        dplsetdisplayduration(dpl.inputnum); break;
	case SDLK_g:        dpl.colmode=COL_G; break;
	case SDLK_b:        dpl.colmode=COL_B; break;
	case SDLK_c:        dpl.colmode=COL_C; break;
	case SDLK_RETURN:   dplmoveabs(dpl.inputnum-1); break;
	case SDLK_DELETE:   if(dpl.writemode) dpldel(); break;
	case SDLK_PLUS:     dplcol(1); break;
	case SDLK_MINUS:    dplcol(-1); break;
	default: break;
	}
	if(key>=SDLK_0 && key<=SDLK_9){
		dpl.inputnum = dpl.inputnum*10 + (key-SDLK_0);
	}else dpl.inputnum=0;
	dpl.showinfo = !dpl.showinfo && key==SDLK_i &&
		dpl.pos.imgi>=0 && dpl.pos.imgi<nimg;
	dpl.showhelp = !dpl.showhelp && key==SDLK_h;
}

char dplev(struct ev *ev){
	switch(ev->ev){
	case DE_RIGHT:
	case DE_LEFT:
	case DE_UP:
	case DE_DOWN:
	case DE_ZOOMIN:
	case DE_ZOOMOUT: dplmove(ev->ev,ev->sx,ev->sy); break;
	case DE_SEL: dplsel(ev->sx,ev->sy); break;
	case DE_ROT1: 
	case DE_ROT2: dplrotate(ev->ev); break;
	case DE_PLAY: 
		if(!panoplay() && dpl.pos.zoom<=0)
			dpl.run=dpl.run ? 0 : -100000;
	break;
	case DE_KEY: dplkey(ev->key); break;
	default: break;
	}
	return dpl.writemode || (dpl.pos.zoom<=0 && ev->key!=SDLK_RIGHT) || dpl.pos.imgi==nimg;
}

void dplcheckev(){
	char stat=-1;
	while(dev.wi!=dev.ri){
		if(stat<0) stat=0;
		stat|=dplev(dev.evs+dev.ri);
		dev.ri=(dev.ri+1)%DPLEVS_NUM;
	}
	if(dev.move.sx){
		dplmovepos(dev.move.sx,dev.move.sy);
		dev.move.sx=0.f;
		dev.move.sy=0.f;
		effinit(EFFREF_IMG,DE_MOVE);
	}
	if(stat>=0) dplstaton(stat);
}

/***************************** dpl run ****************************************/

void dplrun(){
	Uint32 time=SDL_GetTicks();
	if(time-dpl.run>dpl.cfg.displayduration){
		dpl.run=time;
		dplmove(DE_RIGHT,0.f,0.f);
		dplstaton(0);
	}
}

/***************************** eff do *****************************************/

float effcalclin(float a,float b,float ef){
	return (b-a)*ef+a;
}

float effcalcrot(float a,float b,float ef){
	while(b-a> 180.f) b-=360.f;
	while(b-a<-180.f) b+=360.f;
	return (b-a)*ef+a;
}

#define ECS_TIME	0.2f
#define ECS_SIZE	0.3f
float effcalcshrink(float ef){
	if(ef<ECS_TIME)         return 1.f-ef/ECS_TIME*(1.f-ECS_SIZE);
	else if(ef<1.-ECS_TIME) return ECS_SIZE;
	else                    return 1.f-(1.f-ef)/ECS_TIME*(1.f-ECS_SIZE);
}

void effimg(struct imgpos *ip){
	Uint32 time=SDL_GetTicks();
	if(time>=ip->waytime[1]){
		ip->eff=0;
		ip->cur=ip->way[1];
		ip->opt.active=ip->wayact;
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

void effstat(){
	float ef=(float)(SDL_GetTicks()-dpl.stat.in)/(float)(dpl.stat.out-dpl.stat.in);
	if(dpl.stat.mode!=STAT_OFF && ef>=1.f){
		if(dpl.stat.mode!=STAT_ON || dpl.pos.imgi<nimg){
			dpl.stat.mode=(dpl.stat.mode+1)%STAT_NUM;
			dpl.stat.in=dpl.stat.out;
			dpl.stat.out=dpl.stat.in+dpl.cfg.stat_delay[dpl.stat.mode];
		}
	}
	switch(dpl.stat.mode){
		case STAT_RISE: dpl.stat.pos.h=effcalclin(0.f,1.f,ef); break;
		case STAT_FALL: dpl.stat.pos.h=effcalclin(1.f,0.f,ef); break;
		case STAT_ON:   dpl.stat.pos.h=1.f; break;
		default:        dpl.stat.pos.h=0.f; break;
	}
}

void effdo(){
	struct img *img;
	char ineff=0;
	if(!imgs) return;
	for(img=*imgs;img;img=img->nxt) if(img->pos->eff){
		effimg(img->pos);
		ineff=1;
	}
	if(delimg){
		if(delimg->pos->eff) effimg(delimg->pos);
		if(!delimg->pos->eff){
			struct img *tmp=delimg;
			delimg=NULL;
			ldffree(tmp->ld,TEX_NONE);
		}
	}
	effstat();
	dpl.ineff=ineff;
}

/***************************** dpl thread *************************************/

void dplcfginit(){
	dpl.cfg.efftime=cfggetint("dpl.efftime");
	dpl.cfg.displayduration=cfggetint("dpl.displayduration");
	dpl.cfg.shrink=cfggetfloat("dpl.shrink");
	dpl.cfg.loop=cfggetint("dpl.loop");
	dpl.cfg.stat_delay[STAT_OFF] = 0;
	dpl.cfg.stat_delay[STAT_RISE]=cfggetint("dpl.stat_rise");
	dpl.cfg.stat_delay[STAT_ON]  =cfggetint("dpl.stat_on");
	dpl.cfg.stat_delay[STAT_FALL]=cfggetint("dpl.stat_fall");
}

int dplthread(void *arg){
	Uint32 last=0;
	dplcfginit();
	dplstaton(1);
	while(!sdl_quit){

		dplcheckev();
		if(dpl.run) dplrun();
		timer(TI_DPL,0,0);
		if(dpl.refresh!=EFFREF_NO){
			effinit(dpl.refresh,0);
			dpl.refresh=EFFREF_NO;
		}
		timer(TI_DPL,1,0);
		effdo();
		timer(TI_DPL,2,0);
		panorun();
		timer(TI_DPL,3,0);

		sdlthreadcheck();
		timer(TI_DPL,4,0);
		sdldelay(&last,16);
		timer(TI_DPL,5,0);
	}
	sdl_quit|=THR_DPL;
	return 0;
}
