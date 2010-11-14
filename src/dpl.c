#include <stdlib.h>
#include <SDL.h>
#include <math.h>
#include "dpl.h"
#include "sdl.h"
#include "load.h"
#include "cfg.h"
#include "main.h"

struct imgpos {
	char active;
	char eff;
	enum imgtex tex;
	struct ipos cur;
	struct ipos way[2];
	Uint32 waytime[2];
	char   wayact;
};

struct dplpos {
	int imgi;
	int zoom;
	float x,y;
};

struct dpl {
	struct dplpos pos;
	Uint32 run;
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

/***************************** dpl interface **********************************/

int dplgetimgi(){ return dpl.pos.imgi; }
int dplgetzoom(){ return dpl.pos.zoom; }

/***************************** dpl imgpos *************************************/

struct imgpos *imgposinit(){ return calloc(2,sizeof(struct imgpos)); }
void imgposfree(struct imgpos *ip){ free(ip); }

char imgposactive(struct imgpos *ip,int p){ return p<0||p>2 ? 0 : ip[p].active; }
GLuint imgpostex(struct imgpos *ip,int p){ return p<0||p>2 ? 0 : ip[p].tex; }
struct ipos *imgposcur(struct imgpos *ip,int p){ return p<0||p>2 ? NULL : &ip[p].cur; }

/***************************** dpl img move ***********************************/

struct {
	int move;
	float size;
	int inc;
} zoomtab[]={
	{ .move=5, .size=1.,    .inc=0  },
	{ .move=3, .size=1./3., .inc=4  },
	{ .move=5, .size=1./5., .inc=12 },
};

char effact(int i){
	if(i==dpl.pos.imgi) return 1;
	return dpl.pos.zoom<0 && abs(i-dpl.pos.imgi)<=zoomtab[-dpl.pos.zoom].inc;
}

void effwaytime(struct imgpos *ip,Uint32 len){
	ip->waytime[1]=(ip->waytime[0]=SDL_GetTicks())+len;
}

struct ipos effmove(int i){
	struct ipos ipos;
	ipos.a=1.;
	if(dpl.pos.zoom<0){
		ipos.s=zoomtab[-dpl.pos.zoom].size;
		ipos.x=(i-dpl.pos.imgi)*ipos.s;
		ipos.y=0.;
		while(ipos.x<-0.5){ ipos.x+=1.0; ipos.y-=ipos.s; }
		while(ipos.x> 0.5){ ipos.x-=1.0; ipos.y+=ipos.s; }
		if(i!=dpl.pos.imgi) ipos.s*=0.8;
	}else{
		ipos.s=powf(2.,(float)dpl.pos.zoom);
		ipos.x=0.;
		ipos.y=0.;
	}
	printf("=> %.2f %.2f %.2f %.2f\n",ipos.a,ipos.s,ipos.x,ipos.y);
	return ipos;
}

struct ipos effoff(int i){
	struct ipos ipos;
	ipos.a=0.;
	ipos.s=0.;
	ipos.x=0.;
	ipos.y=0.;
	return ipos;
}

struct ipos effon(int i){
	struct ipos ipos;
	ipos.a=0.;
	ipos.s=0.;
	ipos.x=0.;
	ipos.y=0.;
	return ipos;
}

void effinit(int d){
	int i;
	printf("%i %i\n",dpl.pos.imgi,dpl.pos.zoom);
	for(i=0;i<nimg;i++){
		struct imgpos *ip=imgs[i].pos;
		char act = effact(i);
		if(!act && !ip->active) continue;
		printf("%i %3s\n",i,act?"on":"off");
		ip->tex = dpl.pos.zoom>0 ? TEX_FULL : dpl.pos.zoom<0 ? TEX_SMALL : TEX_BIG;
		ip->way[1] = act ? effmove(i) : effoff(i);
		if(!ip->active) ip->cur=ip->way[0]=effon(i);
		else ip->way[0]=ip->cur;
		if(!memcmp(&ip->cur,ip->way+1,sizeof(struct ipos))){
			ip->active=act;
			continue;
		}
		effwaytime(ip,dpl.cfg.efftime);
		ip->wayact=act;
		ip->active=1;
		ip->eff=1;
	}
}

void dplmove(int d){
	switch(abs(d)){
		case 1:
			if(dpl.pos.zoom<=0) dpl.pos.imgi+=d;
			else dpl.pos.x+=0.1*d;
		break;
		case 2:
			if(dpl.pos.zoom<=0) dpl.pos.imgi+=d/2*zoomtab[-dpl.pos.zoom].move;
			else dpl.pos.y+=0.1*(d/2);
		break;
		case 3:
			dpl.pos.zoom+=d/3;
		break;
	}
	if(dpl.pos.zoom<-2)   dpl.pos.zoom=-2;
	if(dpl.pos.imgi<0)    dpl.pos.imgi=0;
	if(dpl.pos.imgi>nimg) dpl.pos.imgi=nimg;
	if(dpl.pos.zoom>0)    dpl.run=0;
	effinit(d);
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
	switch(key.sym){
	case SDLK_q:        sdl.quit=1; break;
	case SDLK_f:        sdl.fullscreen=1; sdl.doresize=1; break;
	case SDLK_m:        sdl.writemode=!sdl.writemode; break;
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

void effimg(struct imgpos *ip){
	Uint32 time=SDL_GetTicks();
	if(time>=ip->waytime[1]){
		ip->eff=0;
		ip->cur=ip->way[1];
		ip->active=ip->wayact;
	}else if(time>ip->waytime[0]){
		float ef = (float)(time-ip->waytime[0]) / (float)(ip->waytime[1]-ip->waytime[0]);
		if(ip->way[0].a!=ip->way[1].a) ip->cur.a=effcalclin(ip->way[0].a,ip->way[1].a,ef);
		if(ip->way[0].s!=ip->way[1].s) ip->cur.s=effcalclin(ip->way[0].s,ip->way[1].s,ef);
		if(ip->way[0].x!=ip->way[1].x) ip->cur.x=effcalclin(ip->way[0].x,ip->way[1].x,ef);
		if(ip->way[0].y!=ip->way[1].y) ip->cur.y=effcalclin(ip->way[0].y,ip->way[1].y,ef);
	}
	/*printf("%i %i %i %g\n",ip->waytime[0],ip->waytime[1],time,ip->cur.a);*/
}

void effdo(){
	int i,p;
	for(i=0;i<nimg;i++) for(p=0;p<2;p++) if(imgs[i].pos[p].eff) effimg(imgs[i].pos+p);
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
		if(dpl.run) dplrun();
		effdo();

		sdldelay(&last,16);
	}
	sdl.quit|=THR_DPL;
	return NULL;
}
