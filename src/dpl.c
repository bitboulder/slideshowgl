#include <stdlib.h>
#include <SDL.h>
#include <math.h>
#include "dpl.h"
#include "sdl.h"
#include "load.h"
#include "cfg.h"
#include "main.h"

struct dplpos {
	int imgi;
	int zoom;
	float x,y;
};

struct dpl {
	struct dplpos pos;
	Uint32 run;
	char refresh;
	struct {
		Uint32 efftime;
		Uint32 playdelay;
	} cfg;
} dpl = {
	.pos.imgi = -1,
	.pos.zoom = 0,
	.pos.x = 0.5,
	.pos.y = 0.5,
	.run = 0,
};

void dplrefresh(){ dpl.refresh=1; }

/***************************** dpl interface **********************************/

int dplgetimgi(){ return dpl.pos.imgi; }
int dplgetzoom(){ return dpl.pos.zoom; }

/***************************** dpl imgpos *************************************/

struct imgpos {
	char eff;
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
	enum imgtex tex;
} zoomtab[]={
	{ .move=5, .size=1.,    .inc=0,  .tex=TEX_BIG   },
	{ .move=3, .size=1./3., .inc=4,  .tex=TEX_SMALL },
	{ .move=5, .size=1./5., .inc=12, .tex=TEX_SMALL },
	{ .move=7, .size=1./7., .inc=24, .tex=TEX_TINY  },
};

char effact(int i){
	if(i==dpl.pos.imgi) return 1;
	return dpl.pos.zoom<0 && abs(i-dpl.pos.imgi)<=zoomtab[-dpl.pos.zoom].inc;
}

void effwaytime(struct imgpos *ip,Uint32 len){
	ip->waytime[1]=(ip->waytime[0]=SDL_GetTicks())+len;
}

void effmove(struct ipos *ip,int i){
	ip->a=1.;
	if(dpl.pos.zoom<0){
		ip->s=zoomtab[-dpl.pos.zoom].size;
		ip->x=(i-dpl.pos.imgi)*ip->s;
		ip->y=0.;
		while(ip->x<-0.5){ ip->x+=1.0; ip->y-=ip->s; }
		while(ip->x> 0.5){ ip->x-=1.0; ip->y+=ip->s; }
		if(i!=dpl.pos.imgi) ip->s*=0.8;
	}else{
		ip->s=powf(2.,(float)dpl.pos.zoom);
		ip->x=0.;
		ip->y=0.;
	}
	/*printf("=> %.2f %.2f %.2f %.2f\n",ip->a,ip->s,ip->x,ip->y);*/
}

char effonoff(struct ipos *ip,struct ipos *ipon,int d){
	if(dpl.pos.zoom>0 || abs(d)==3){
		memset(ip,0,sizeof(struct ipos));
		return 1;
	}
	*ip=*ipon;
	if(dpl.pos.zoom==0){
		if(sdl.writemode) ip->x += d<0?-1.:1.;
		else ip->a=0.;
	}else{
		ip->a=0.;
		switch(abs(d)){
		case 1: ip->x += (ip->x<0.?-1.:1.) * zoomtab[-dpl.pos.zoom].size; break;
		case 2: ip->y += (ip->y<0.?-1.:1.) * zoomtab[-dpl.pos.zoom].size; break;
		}
	}
	return 0;
}

void imgfit(struct imgpos *ip,float irat){
	float srat=(float)sdl.scr_h/(float)sdl.scr_w;
	if(srat<irat){
		ip->opt.fitw=srat/irat;
		ip->opt.fith=1.;
	}else{
		ip->opt.fitw=1.;
		ip->opt.fith=irat/srat;
	}
	/*printf("%g %g (%g %g)\n",il->fitw,il->fith,irat,srat);*/
}

enum imgtex imgseltex(){
	if(dpl.pos.zoom>0)  return TEX_FULL;
	if(dpl.pos.zoom==0) return zoomtab[-dpl.pos.zoom].tex;
	return TEX_TINY;
}

void effinit(int d){
	int i;
	/*printf("%i %i\n",dpl.pos.imgi,dpl.pos.zoom);*/
	for(i=0;i<nimg;i++){
		struct imgpos *ip=imgs[i].pos;
		char act = effact(i);
		if(!act && !ip->opt.active) continue;
		imgfit(ip,imgldrat(imgs[i].ld));
		/*printf("%i %3s\n",i,act?"on":"off");*/
		ip->opt.tex=imgseltex();
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
			continue;
		}
		effwaytime(ip,dpl.cfg.efftime);
		ip->wayact=act;
		ip->opt.active=1;
		ip->eff=1;
	}
}

void dplmove(int d){
	const static int zoommin=sizeof(zoomtab)/sizeof(struct zoomtab);
	if(d){
		switch(abs(d)){
		case 1:
			if(dpl.pos.zoom<=0) dpl.pos.imgi+=d;
			else dpl.pos.x+=0.1*d;
		break;
		case 2:
			if(dpl.pos.zoom<0)  dpl.pos.imgi-=d/2*zoomtab[-dpl.pos.zoom].move;
			if(dpl.pos.zoom==0) dpl.pos.imgi+=d/2*zoomtab[-dpl.pos.zoom].move;
			else dpl.pos.y+=0.1*(d/2);
		break;
		case 3:
			dpl.pos.zoom+=d/3;
		break;
		}
		if(dpl.pos.zoom<1-zoommin) dpl.pos.zoom=1-zoommin;
		if(dpl.pos.imgi<0)    dpl.pos.imgi=0;
		if(dpl.pos.imgi>nimg) dpl.pos.imgi=nimg;
		if(dpl.pos.zoom>0)    dpl.run=0;
		debug(DBG_STA,"dpl move => imgi %i zoom %i pos %.2fx%.2f",dpl.pos.imgi,dpl.pos.zoom,dpl.pos.x,dpl.pos.y);
	}else{
		imgfit(defimg.pos,imgldrat(defimg.ld));
		debug(DBG_STA,"dpl move refresh");
	}
	effinit(d);
	dpl.refresh=0;
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

void dplkey(SDL_keysym key){
	debug(DBG_STA,"dpl key %i",key.sym);
	switch(key.sym){
	case SDLK_q:        sdl.quit=1; break;
	case SDLK_f:        sdl.fullscreen=1; sdl.doresize=1; break;
	case SDLK_w:        sdl.writemode=!sdl.writemode; break;
	case SDLK_RIGHT:    dplmove( 1); break;
	case SDLK_LEFT:     dplmove(-1); break;
	case SDLK_UP:       dplmove( 2); break;
	case SDLK_DOWN:     dplmove(-2); break;
	case SDLK_PAGEUP:   dplmove( 3); break;
	case SDLK_PAGEDOWN: dplmove(-3); break;
	case SDLK_SPACE:    dpl.run=dpl.run ? 0 : -100000; break;
	default: break;
	}
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
	if(time-dpl.run>dpl.cfg.playdelay){
		dpl.run=time;
		dplmove(1);
	}
}

/***************************** eff do *****************************************/

float effcalclin(float a,float b,float ef){
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
		if(ip->opt.back) ip->cur.s*=effcalcshrink(ef);
	}
	/*printf("%i %i %i %g\n",ip->waytime[0],ip->waytime[1],time,ip->cur.a);*/
}

void effdo(){
	int i;
	for(i=0;i<nimg;i++) if(imgs[i].pos->eff) effimg(imgs[i].pos);
}

/***************************** dpl thread *************************************/

void dplcfginit(){
	dpl.cfg.efftime=cfggetint("dpl.efftime");
	dpl.cfg.playdelay=cfggetint("dpl.playdelay");
}

void *dplthread(void *arg){
	Uint32 last=0;
	dplcfginit();
	while(!sdl.quit){

		dplcheckkey();
		if(dpl.refresh) dplmove(0);
		if(dpl.run) dplrun();
		effdo();

		sdldelay(&last,16);
	}
	sdl.quit|=THR_DPL;
	return NULL;
}
