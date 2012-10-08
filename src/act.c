#include <stdlib.h>
#include <SDL.h>
#include "act.h"
#include "main.h"
#include "sdl.h"
#include "load.h"
#include "cfg.h"
#include "exif.h"
#include "file.h"
#include "eff.h"
#include "mark.h"
#include "map.h"
#include "help.h"
#include "exich.h"

struct actdelay {
	Uint32 delay;
	Uint32 countdown;
};

struct actcfg {
	struct actdelay delay[ACT_NUM];
	char runcmd;
};

struct ac {
	struct actcfg cfg;
	SDL_mutex *mx_do;
	SDL_mutex *mx_dl;
	char init;
} ac = {
	.init = 0,
};


char runcmd(char *cmd){
	debug(DBG_NONE,"running cmd: %s",cmd);
	if(ac.cfg.runcmd) return system(cmd)==0;
	return 1;
}

void actrotate(struct img *img){
	const char *fn=imgfilefn(img->file);
	float rot=imgexifrotf(img->exif);
	char cmd[FILELEN+64];
	char done;
	snprintf(cmd,FILELEN+64,"myjpegtool -x %.0f -w \"%s\"",rot,fn);
	done=runcmd(cmd);
	if(!done){
		snprintf(cmd,FILELEN+64,"myjpegtool -x %.0f -s -w \"%s\"",rot,fn);
		done=runcmd(cmd);
	}
	if(done) imgldfiletime(img->ld,FT_UPDATE);
	if(done) debug(DBG_STA,"img rotated (%s)",fn);
	else error(ERR_CONT,"img rotating failed (%s)",fn);
}

void actdelete(struct img *img,struct imglist *il,const char *dstdir){
	static char dir[FILELEN];
	char *pos;
	char *ifn=imgfiledelfn(img->file);
	char ifnreset=1;
	if(!ifn || !ifn[0]){
		ifn=imgfilefn(img->file);
		ifnreset=0;
	}
	snprintf(dir,FILELEN,ifn);
	if((pos=strrchr(dir,'/'))) pos++; else pos=dir;
	snprintf(pos,FILELEN-(size_t)(pos-dir),dstdir);
	frename(ifn,dir);
	if(ifnreset) ifn[0]='\0';
	if(il) ilfiletime(il,FT_UPDATE);
	debug(DBG_STA,"img deleted to %s (%s)",dstdir,ifn);
}

void actdo(enum act act,struct img *img,struct imglist *il){
	debug(DBG_STA,"action %i run immediatly",act);
	SDL_mutexP(ac.mx_do);
	switch(act){
	case ACT_SAVEMARKS: markssave(); break;
	case ACT_ROTATE:    actrotate(img); break;
	case ACT_DELETE:    actdelete(img,il,"del"); break;
	case ACT_DELORI:    actdelete(img,il,"ori"); break;
	case ACT_ILCLEANUP: ilscleanup(); break;
	case ACT_MAPCLT:    mapimgclt(-1); break;
	case ACT_EXIFCACHE: exichsave(); break;
	default: break;
	}
	SDL_mutexV(ac.mx_do);
}

void actdelay(enum act act){
	struct actdelay *dl=ac.cfg.delay+act;
	SDL_mutexP(ac.mx_dl);
	if(!dl->countdown){
		debug(DBG_STA,"action %i run with delay",act);
		dl->countdown=SDL_GetTicks()+dl->delay;
	}
	SDL_mutexV(ac.mx_dl);
}

void actrun(enum act act,struct img *img,struct imglist *il){
	Uint32 delay=ac.cfg.delay[act].delay;
	if(delay) actdelay(act);
	else actdo(act,img,il);
}

void actcheckdelay(char force){
	int i;
	struct actdelay *dl=ac.cfg.delay;
	SDL_mutexP(ac.mx_dl);
	for(i=0;i<ACT_NUM;i++,dl++) if(dl->countdown && (force || SDL_GetTicks()>=dl->countdown)){
		dl->countdown=0;
		SDL_mutexV(ac.mx_dl);
		actdo(i,NULL,NULL);
		SDL_mutexP(ac.mx_dl);
	}
	SDL_mutexV(ac.mx_dl);
}

#define QACT_NUM	16
struct qact {
	enum act act;
	struct img *img;
	struct imglist *il;
} qacts[QACT_NUM];
int qact_wi=0;
int qact_ri=0;

/* thread: dpl, load, main */
void actadd(enum act act,struct img *img,struct imglist *il){
	int nwi=(qact_wi+1)%QACT_NUM;
	if(!ac.init) return;
	if(ac.cfg.delay[act].delay) actdelay(act); else{
		while(nwi==qact_ri) SDL_Delay(100);
		qacts[qact_wi].act=act;
		qacts[qact_wi].img=img;
		qacts[qact_wi].il=il;
		qact_wi=nwi;
	}
}

char actpop(){
	if(qact_wi==qact_ri) return 0;
	actrun(qacts[qact_ri].act,qacts[qact_ri].img,qacts[qact_ri].il);
	qact_ri=(qact_ri+1)%QACT_NUM;
	return 1;
}

void actinit(){
	ac.mx_do=SDL_CreateMutex();
	ac.mx_dl=SDL_CreateMutex();
	ac.cfg.runcmd = cfggetbool("act.do");
	memset(ac.cfg.delay,0,sizeof(struct actdelay)*ACT_NUM);
	ac.cfg.delay[ACT_SAVEMARKS].delay = cfggetuint("act.savemarks_delay");
	ac.cfg.delay[ACT_ILCLEANUP].delay = cfggetuint("act.ilcleanup_delay");
	ac.cfg.delay[ACT_MAPCLT].delay    = cfggetuint("act.mapclt_delay");
	ac.cfg.delay[ACT_EXIFCACHE].delay = cfggetuint("act.exifcache_delay");
	ac.init=1;
}

int actthread(void *UNUSED(arg)){
	mapinit();
	while(!sdl_quit){
		actcheckdelay(0);
		if(!actpop()) SDL_Delay(500);
		timer(TI_THR,0,1);
	}
	SDL_Delay(20);
	actcheckdelay(1);
	sdl_quit|=THR_ACT;
	return 0;
}

/* thread: any */
void actforce(){
	if(!ac.init) return;
	actcheckdelay(1);
}
