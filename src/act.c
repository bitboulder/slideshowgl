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

struct actdelay {
	Uint32 delay;
	Uint32 countdown;
};

struct actcfg {
	struct actdelay delay[ACT_NUM];
	char runcmd;
} actcfg;


void runcmd(char *cmd){
	debug(DBG_NONE,"running cmd: %s",cmd);
	if(actcfg.runcmd) system(cmd);
}

void actloadmarks(){
	const char *fn=cfggetstr("act.mark_fn");
	FILE *fd;
	char line[FILELEN];
	/* TODO: what to do with marks and mulitple img lists */
	if(!(fd=fopen(fn,"r"))) return;
	while(!feof(fd) && fgets(line,FILELEN,fd)){
		struct img *img;
		int len=(int)strlen(line);
		while(len && (line[len-1]=='\n' || line[len-1]=='\r')) line[--len]='\0';
		for(img=imgget(0);img;img=img->nxt)
			if(!strcmp(imgfilefn(img->file),line))
				*imgposmark(img->pos)=1;
	}
	fclose(fd);
	debug(DBG_STA,"marks loaded");
}

void actsavemarks(){
	const char *fn=cfggetstr("act.mark_fn");
	FILE *fd;
	struct img *img;
	/* TODO: what to do with marks and mulitple img lists */
	if(!(fd=fopen(fn,"w"))) return;
	for(img=imgget(0);img;img=img->nxt) if(*imgposmark(img->pos))
		fprintf(fd,"%s\n",imgfilefn(img->file));
	fclose(fd);
	debug(DBG_STA,"marks saved");
}

void actrotate(struct img *img){
	char *fn=imgfilefn(img->file);
	float rot=imgexifrotf(img->exif);
	char cmd[FILELEN+64];
	snprintf(cmd,FILELEN+64,"myjpegtool -x %.0f -w \"%s\"",rot,fn);
	runcmd(cmd);
	debug(DBG_STA,"img rotated (%s)",fn);
}

void actdelete(struct img *img){
	static char fn[FILELEN];
	static char cmd[FILELEN*2+16];
	char *pos;
	snprintf(fn,FILELEN-4,imgfilefn(img->file));
	if((pos=strrchr(fn,'/'))) pos++; else pos=fn;
	memmove(pos+4,pos,strlen(pos)+1);
	strcpy(pos,"del");
	snprintf(cmd,FILELEN*2+16,"mkdir -p \"%s\"",fn);
	runcmd(cmd);
	pos[3]='/';
	snprintf(cmd,FILELEN*2+16,"mv \"%s\" \"%s\"",imgfilefn(img->file),fn);
	runcmd(cmd);
	debug(DBG_STA,"img deleted (%s)",fn);
}

void actdo(enum act act,struct img *img){
	switch(act){
	case ACT_LOADMARKS: actloadmarks(); break;
	case ACT_SAVEMARKS: actsavemarks(); break;
	case ACT_ROTATE:    actrotate(img); break;
	case ACT_DELETE:    actdelete(img); break;
	case ACT_ILCLEANUP: ilcleanup(); break;
	default: break;
	}
}

void actrun(enum act act,struct img *img){
	struct actdelay *dl=actcfg.delay+act;
	debug(DBG_STA,"action %i run %s",act,dl->delay?"with delay":"immediatly");
	if(!dl->delay) actdo(act,img);
	else if(!dl->countdown) dl->countdown=SDL_GetTicks()+dl->delay;
}

void actcheckdelay(char force){
	int i;
	struct actdelay *dl=actcfg.delay;
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
	actcfg.runcmd = cfggetbool("act.do");
	memset(actcfg.delay,0,sizeof(struct actdelay)*ACT_NUM);
	actcfg.delay[ACT_SAVEMARKS].delay = cfggetuint("act.savemarks_delay");
	actcfg.delay[ACT_ILCLEANUP].delay = cfggetuint("act.ilcleanup_delay");
}

int actthread(void *UNUSED(arg)){
	actinit();
	while(!sdl_quit){
		actcheckdelay(0);
		if(!actpop()) SDL_Delay(500);
	}
	actcheckdelay(1);
	sdl_quit|=THR_ACT;
	return 0;
}
