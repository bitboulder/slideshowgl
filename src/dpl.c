#include <stdlib.h>
#include <SDL.h>
#include <math.h>
#include "dpl.h"
#include "sdl.h"
#include "load.h"
#include "cfg.h"
#include "main.h"
#include "exif.h"

struct dplpos {
	int imgi;
	int imgiold;
	int zoom;
	float x,y;
};

struct dpl {
	struct dplpos pos;
	float maxfitw,maxfith;
	Uint32 run;
	enum dplrefresh refresh;
	char ineff;
	char showinfo;
	char showhelp;
	int inputnum;
	struct {
		Uint32 efftime;
		Uint32 displayduration;
		float shrink;
		char loop;
	} cfg;
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
	.refresh = DPLREF_NO,
};

/***************************** dpl interface **********************************/

void dplrefresh(enum dplrefresh val){ dpl.refresh=val; }
int dplgetimgi(){ return dpl.pos.imgi; }
int dplgetzoom(){ return dpl.pos.zoom; }
char dplineff(){ return dpl.ineff; }
char dplshowinfo(){ return dpl.showinfo; }
int dplinputnum(){ return dpl.inputnum; }
char dplloop(){ return dpl.cfg.loop; }

/***************************** dpl imgpos *************************************/

struct imgpos {
	char eff;
	char mark;
	struct iopt opt;
	struct ipos cur;
	struct ipos way[2];
	Uint32 waytime[2];
	char   wayact;
};

struct imgpos *imgposinit(){ return calloc(1,sizeof(struct imgpos)); }
void imgposfree(struct imgpos *ip){ free(ip); }

struct iopt *imgposopt(struct imgpos *ip){ return &ip->opt; }
struct ipos *imgposcur(struct imgpos *ip){ return &ip->cur; }

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
	ip->a=1.;
	ip->m=(imgs[i].pos->mark && sdl.writemode)?1.:0.;
	ip->r=imgexifrotf(imgs[i].exif);
	if(dpl.pos.zoom<0){
		ip->s=zoomtab[-dpl.pos.zoom].size;
		ip->x=(i-dpl.pos.imgi)*ip->s;
		ip->y=0.;
		while(ip->x<-0.5){ ip->x+=1.0; ip->y-=ip->s; }
		while(ip->x> 0.5){ ip->x-=1.0; ip->y+=ip->s; }
		if(i!=dpl.pos.imgi) ip->s*=dpl.cfg.shrink;
		ip->x*=dpl.maxfitw;
		ip->y*=dpl.maxfith;
	}else{
		ip->s=powf(2.,(float)dpl.pos.zoom);
		ip->x=dpl.pos.x;
		ip->y=dpl.pos.y;
	}
	/*printf("=> %.2f %.2f %.2f %.2f\n",ip->a,ip->s,ip->x,ip->y);*/
}

char effonoff(struct ipos *ip,struct ipos *ipon,int d){
	if(dpl.pos.zoom>0 || abs(d)==3){
		memset(ip,0,sizeof(struct ipos));
		ip->m=ipon->m;
		ip->r=ipon->r;
		return 1;
	}
	*ip=*ipon;
	if(dpl.pos.imgi>=nimg || dpl.pos.imgiold>=nimg){
		ip->a=0.;
	}else if(dpl.pos.zoom==0){
		if(sdl.writemode) switch(d){
			case -2: ip->x -= zoomtab[0].move; break;
			case -1: ip->x -= 1.; break;
			case  1: ip->x += 1.; break;
			case  2: ip->x += zoomtab[0].move; break;
		}else ip->a=0.;
	}else{
		ip->a=0.;
		switch(abs(d)){
		case 1: ip->x += (ip->x<0.?-1.:1.) * zoomtab[-dpl.pos.zoom].size; break;
		case 2: ip->y += (ip->y<0.?-1.:1.) * zoomtab[-dpl.pos.zoom].size; break;
		}
	}
	return 0;
}

char efffaston(struct imgpos *ip,int d,int i){
	int diff=dpl.pos.imgi-i;
	if((d!=-2 || diff>=0 || diff<=-zoomtab[0].move) &&
	   (d!= 2 || diff<=0 || diff>= zoomtab[0].move)) return 0;
	ip->opt.active=2;
	ip->cur.a=1.;
	ip->cur.s=1.;
	ip->cur.x=((d<0?-1.:1.) * zoomtab[0].move) - (float)diff;
	ip->cur.y=0.;
	ip->cur.m=(imgs[i].pos->mark && sdl.writemode)?1.:0.;
	ip->cur.r=imgexifrotf(imgs[i].exif);
	ip->eff=0;
	return 1;
}

enum imgtex imgseltex(struct imgpos *ip,int i){
	if(dpl.pos.zoom>0)  return TEX_FULL;
	if(dpl.pos.imgi==i) return zoomtab[-dpl.pos.zoom].texcur;
	else if(ip->opt.active==2) return TEX_SMALL;
	else return zoomtab[-dpl.pos.zoom].texoth;
}

void effinitimg(int d,int i){
	struct imgpos *ip=imgs[i].pos;
	char act = effact(i);
	if(!act && !ip->opt.active){
		if(dpl.pos.zoom!=0 || abs(d)!=2 || !sdl.writemode) return;
		if(!efffaston(ip,d,i)) return;
	}
	if(!act && ip->eff && !ip->wayact) return;
	/*printf("%i %3s\n",i,act?"on":"off");*/
	ip->opt.tex=imgseltex(ip,i);
	ip->opt.back=0;
	if(act) effmove(ip->way+1,i);
	else  ip->opt.back|=effonoff(ip->way+1,&ip->cur,-d);
	if(!ip->opt.active){
		ip->opt.back|=effonoff(ip->way+0,ip->way+1,d);
		ip->cur=ip->way[0];
	}else{
		ip->way[0]=ip->cur;
		if(act && dpl.pos.zoom<0 &&
			(fabs(ip->way[0].x-ip->way[1].x)>.5 ||
			 fabs(ip->way[0].y-ip->way[1].y)>.5)
			) ip->opt.back=1;
	}
	if(!memcmp(&ip->cur,ip->way+1,sizeof(struct ipos))){
		ip->opt.active=act;
		return;
	}
	effwaytime(ip,dpl.cfg.efftime);
	ip->wayact=act;
	ip->opt.active=1;
	ip->eff=1;
}

void effmaxfit(){
	int i;
	dpl.maxfitw=dpl.maxfith=0.;
	for(i=0;i<nimg;i++) if(effact(i)){
		struct imgpos *ip=imgs[i].pos;
		if(!ip->opt.fitw || !ip->opt.fith) continue;
		if(ip->opt.fitw>dpl.maxfitw) dpl.maxfitw=ip->opt.fitw;
		if(ip->opt.fith>dpl.maxfith) dpl.maxfith=ip->opt.fith;
	}
	if(dpl.maxfitw==0.) dpl.maxfitw=1.;
	if(dpl.maxfith==0.) dpl.maxfith=1.;
}

void effinit(int d){
	int i;
	if(dpl.pos.zoom<0) effmaxfit();
	for(i=0;i<nimg;i++) effinitimg(d,i);
}

char imgfit(struct img *img){
	float irat=imgldrat(img->ld);
	float srat=(float)sdl.scr_h/(float)sdl.scr_w;
	struct iopt *iopt=&img->pos->opt;
	enum rot rot=imgexifrot(img->exif);
	if(!irat) return 0;
	if(rot==ROT_90 || rot==ROT_270) irat=1./irat;
	if(srat<irat){
		iopt->fitw=srat/irat;
		iopt->fith=1.;
	}else{
		iopt->fitw=1.;
		iopt->fith=irat/srat;
	}
	if(iopt->active && dpl.pos.zoom<0 && 
			(iopt->fitw>dpl.maxfitw || iopt->fith>dpl.maxfith)
		) dplrefresh(DPLREF_STD);
	debug(DBG_DBG,"imgfit %.2fx%.2f %s",iopt->fitw,iopt->fith,imgldfn(img->ld));
	/*printf("%g %g (%g %g)\n",il->fitw,il->fith,irat,srat);*/
	return 1;
}

void dplfitrefresh(){
	int i;
	imgfit(&defimg);
	for(i=0;i<nimg;i++) imgfit(imgs+i);
}

void dplmove(int d){
	const static int zoommin=sizeof(zoomtab)/sizeof(struct zoomtab);
	dpl.pos.imgiold=dpl.pos.imgi;
	if(d){
		switch(abs(d)){
		case 1:
			if(dpl.pos.zoom<=0) dpl.pos.imgi+=d;
			else dpl.pos.x-=(float)d*.25;
		break;
		case 2:
			if(dpl.pos.zoom<0)  dpl.pos.imgi-=d/2*zoomtab[-dpl.pos.zoom].move;
			if(dpl.pos.zoom==0) dpl.pos.imgi+=d/2*zoomtab[-dpl.pos.zoom].move;
			else dpl.pos.y+=(float)(d/2)*.25;
		break;
		case 3:
			dpl.pos.zoom+=d/3;
			if(dpl.pos.zoom<=0)   dpl.pos.x=dpl.pos.y=0.;
			else{
				dpl.pos.x*=powf(2,(float)(d/3));
				dpl.pos.y*=powf(2,(float)(d/3));
			}
		break;
		}
		if(dpl.pos.zoom<1-zoommin) dpl.pos.zoom=1-zoommin;
		if(dpl.cfg.loop){
			while(dpl.pos.imgi<0)     dpl.pos.imgi+=nimg;
			while(dpl.pos.imgi>=nimg) dpl.pos.imgi-=nimg;
		}else{
			if(dpl.pos.imgi<0)    dpl.pos.imgi=0;
			if(dpl.pos.imgi>nimg) dpl.pos.imgi=nimg;
		}
		if(dpl.pos.zoom>0)    dpl.run=0;
		debug(DBG_STA,"dpl move => imgi %i zoom %i pos %.2fx%.2f",dpl.pos.imgi,dpl.pos.zoom,dpl.pos.x,dpl.pos.y);
	}
	if(dpl.refresh!=DPLREF_NO){
		debug(DBG_STA,"dpl move refresh%s",dpl.refresh==DPLREF_FIT?" (fit)":"");
		if(dpl.refresh==DPLREF_FIT) dplfitrefresh();
		dpl.refresh=DPLREF_NO;
	}
	effinit(d);
}

void dplmoveabs(int imgi){
	if(imgi<0 || imgi>nimg) return;
	dpl.pos.imgi=imgi;
	effinit(0);
}

void dplmark(){
	struct img *img=imgget(dpl.pos.imgi);
	if(!img) return;
	img->pos->mark=!img->pos->mark;
	effinitimg(0,dpl.pos.imgi);
}

void dplrotate(int r){
	struct img *img=imgget(dpl.pos.imgi);
	if(!img) return;
	exifrotate(img->exif,r);
	imgfit(img);
	effinitimg(0,dpl.pos.imgi);
}

/***************************** dpl action *************************************/

void dplsetdisplayduration(int dur){
	if(dur<100) dur*=1000;
	if(dur>=250 && dur<=30000) dpl.cfg.displayduration=dur;
}

/***************************** dpl key ****************************************/

#define DPLKEYS_NUM	32
struct dplkeys {
	SDL_keysym keys[DPLKEYS_NUM];
	int wi,ri;
} dk = {
	.wi = 0,
	.ri = 0,
};

void dplkeyput(SDL_keysym key){
	int nwi=(dk.wi+1)%DPLKEYS_NUM;
	dk.keys[dk.wi]=key;
	if(nwi!=dk.ri) dk.wi=nwi;
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

void dplkey(SDL_keysym key){
	debug(DBG_STA,"dpl key %i",key.sym);
	switch(key.sym){
	case SDLK_ESCAPE:   if(dpl.inputnum || dpl.showinfo || dpl.showhelp) break;
	case SDLK_q:        sdl.quit=1; break;
	case SDLK_f:        sdlfullscreen(); break;
	case SDLK_w:        sdl.writemode=!sdl.writemode; dplrefresh(DPLREF_STD); break;
	case SDLK_m:        if(sdl.writemode) dplmark(); break;
	case SDLK_d:        dplsetdisplayduration(dpl.inputnum); break;
	case SDLK_r:        dplrotate((key.mod&(KMOD_LSHIFT|KMOD_RSHIFT))?-1:1); break;
	case SDLK_RETURN:   dplmoveabs(dpl.inputnum-1); break;
	case SDLK_RIGHT:    dplmove( 1); break;
	case SDLK_LEFT:     dplmove(-1); break;
	case SDLK_UP:       dplmove( 2); break;
	case SDLK_DOWN:     dplmove(-2); break;
	case SDLK_PAGEUP:   dplmove( 3); break;
	case SDLK_PAGEDOWN: dplmove(-3); break;
	case SDLK_SPACE:    dpl.run=dpl.run ? 0 : -100000; break;
	default: break;
	}
	if(key.sym>=SDLK_0 && key.sym<=SDLK_9){
		dpl.inputnum = dpl.inputnum*10 + (key.sym-SDLK_0);
	}else dpl.inputnum=0;
	dpl.showinfo = !dpl.showinfo && key.sym==SDLK_i &&
		dpl.pos.imgi>=0 && dpl.pos.imgi<nimg;
	dpl.showhelp = !dpl.showhelp && key.sym==SDLK_h;
}

void dplcheckkey(){
	while(dk.wi!=dk.ri){
		dplkey(dk.keys[dk.ri]);
		dk.ri=(dk.ri+1)%DPLKEYS_NUM;
	}
}

/***************************** dpl run ****************************************/

void dplrun(){
	Uint32 time=SDL_GetTicks();
	if(time-dpl.run>dpl.cfg.displayduration){
		dpl.run=time;
		dplmove(1);
	}
}

/***************************** eff do *****************************************/

float effcalclin(float a,float b,float ef){
	return (b-a)*ef+a;
}

float effcalcrot(float a,float b,float ef){
	while(b-a> 180.) b-=360.;
	while(b-a<-180.) b+=360.;
	return (b-a)*ef+a;
}

#define ECS_TIME	0.2
#define ECS_SIZE	0.3
float effcalcshrink(float ef){
	if(ef<ECS_TIME)         return 1.-ef/ECS_TIME*(1.-ECS_SIZE);
	else if(ef<1.-ECS_TIME) return ECS_SIZE;
	else                    return 1.-(1.-ef)/ECS_TIME*(1.-ECS_SIZE);
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

void effdo(){
	int i;
	char ineff=0;
	for(i=0;i<nimg;i++) if(imgs[i].pos->eff){
		effimg(imgs[i].pos);
		ineff=1;
	}
	dpl.ineff=ineff;
}

/***************************** dpl thread *************************************/

void dplcfginit(){
	dpl.cfg.efftime=cfggetint("dpl.efftime");
	dpl.cfg.displayduration=cfggetint("dpl.displayduration");
	dpl.cfg.shrink=cfggetfloat("dpl.shrink");
	dpl.cfg.loop=cfggetint("dpl.loop");
}

void *dplthread(void *arg){
	Uint32 last=0;
	dplcfginit();
	while(!sdl.quit){

		dplcheckkey();
		if(dpl.run) dplrun();
		if(dpl.refresh!=DPLREF_NO) dplmove(0);
		effdo();

		sdlthreadcheck();
		sdldelay(&last,16);
	}
	sdl.quit|=THR_DPL;
	return NULL;
}
