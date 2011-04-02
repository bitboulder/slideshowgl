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
	SDL_mutex *mutex;
} ac = {
	.mutex = NULL,
};


char runcmd(char *cmd){
	debug(DBG_NONE,"running cmd: %s",cmd);
	if(ac.cfg.runcmd) return system(cmd)==0;
	return 1;
}

void actrotate(struct img *img){
	char *fn=imgfilefn(img->file);
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

void actdelete(struct img *img,const char *dstdir){
	static char fn[FILELEN];
	static char cmd[FILELEN*2+16];
	char *pos;
	char *ifn=imgfiledelfn(img->file);
	char ifnreset=1;
	if(!ifn || !ifn[0]){
		ifn=imgfilefn(img->file);
		ifnreset=0;
	}
	snprintf(fn,FILELEN-4,ifn);
	if((pos=strrchr(fn,'/'))) pos++; else pos=fn;
	memmove(pos+4,pos,strlen(pos)+1);
	strcpy(pos,dstdir);
	snprintf(cmd,FILELEN*2+16,"mkdir -p \"%s\"",fn);
	runcmd(cmd);
	pos[3]='/';
	snprintf(cmd,FILELEN*2+16,"mv \"%s\" \"%s\"",ifn,fn);
	if(ifnreset) ifn[0]='\0';
	runcmd(cmd);
	debug(DBG_STA,"img deleted (%s)",fn);
}

void actdo(enum act act,struct img *img){
	switch(act){
	case ACT_SAVEMARKS: markssave(); break;
	case ACT_ROTATE:    actrotate(img); break;
	case ACT_DELETE:    actdelete(img,"del"); break;
	case ACT_DELORI:    actdelete(img,"ori"); break;
	case ACT_ILCLEANUP: ilcleanup(); break;
	default: break;
	}
}

void actrun(enum act act,struct img *img){
	struct actdelay *dl=ac.cfg.delay+act;
	debug(DBG_STA,"action %i run %s",act,dl->delay?"with delay":"immediatly");
	if(!dl->delay) actdo(act,img);
	else if(!dl->countdown) dl->countdown=SDL_GetTicks()+dl->delay;
}

void actcheckdelay(char force){
	int i;
	struct actdelay *dl=ac.cfg.delay;
	for(i=0;i<ACT_NUM;i++,dl++) if(dl->countdown && (force || SDL_GetTicks()>=dl->countdown)){
		dl->countdown=0;
		actdo(i,NULL);
	}
}

#define QACT_NUM	16
struct qact {
	enum act act;
	struct img *img;
} qacts[QACT_NUM];
int qact_wi=0;
int qact_ri=0;

/* thread: dpl, load, main */
void actadd(enum act act,struct img *img){
	int nwi=(qact_wi+1)%QACT_NUM;
	while(nwi==qact_ri) SDL_Delay(100);
	qacts[qact_wi].act=act;
	qacts[qact_wi].img=img;
	qact_wi=nwi;
}

char actpop(){
	if(qact_wi==qact_ri) return 0;
	actrun(qacts[qact_ri].act,qacts[qact_ri].img);
	qact_ri=(qact_ri+1)%QACT_NUM;
	return 1;
}

void actinit(){
	ac.mutex=SDL_CreateMutex();
	SDL_mutexP(ac.mutex);
	ac.cfg.runcmd = cfggetbool("act.do");
	memset(ac.cfg.delay,0,sizeof(struct actdelay)*ACT_NUM);
	ac.cfg.delay[ACT_SAVEMARKS].delay = cfggetuint("act.savemarks_delay");
	ac.cfg.delay[ACT_ILCLEANUP].delay = cfggetuint("act.ilcleanup_delay");
}

int actthread(void *UNUSED(arg)){
	actinit();
	while(!sdl_quit){
		actcheckdelay(0);
		if(!actpop()){
			SDL_mutexV(ac.mutex);
			SDL_Delay(500);
			SDL_mutexP(ac.mutex);
		}
	}
	SDL_Delay(20);
	actcheckdelay(1);
	sdl_quit|=THR_ACT;
	return 0;
}

/* thread: any */
void actforce(){
	if(!ac.mutex) return;
	SDL_mutexP(ac.mutex);
	actcheckdelay(1);
	SDL_mutexV(ac.mutex);
}
