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

struct actcfg {
	Uint32 savemarks_delay;
	Uint32 savemarks;
	char runcmd;
} actcfg = {
	.savemarks = 0,
};

void runcmd(char *cmd){
	debug(DBG_NONE,"running cmd: %s",cmd);
	if(actcfg.runcmd) system(cmd);
}

void actloadmarks(){
	const char *fn=cfggetstr("act.mark_fn");
	FILE *fd;
	char line[FILELEN];
	if(!(fd=fopen(fn,"r"))) return;
	while(!feof(fd) && fgets(line,FILELEN,fd)){
		struct img *img;
		int len=(int)strlen(line);
		while(len && (line[len-1]=='\n' || line[len-1]=='\r')) line[--len]='\0';
		for(img=*imgs;img;img=img->nxt)
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
	if(!(fd=fopen(fn,"w"))) return;
	for(img=*imgs;img;img=img->nxt) if(*imgposmark(img->pos))
		fprintf(fd,"%s\n",imgfilefn(img->file));
	fclose(fd);
	debug(DBG_STA,"marks saved");
}

void acttrysavemarks(){
	if(!actcfg.savemarks || SDL_GetTicks()>actcfg.savemarks) return;
	actcfg.savemarks=0;
	actsavemarks();
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
	case ACT_SAVEMARKS: if(!actcfg.savemarks) actcfg.savemarks=SDL_GetTicks()+actcfg.savemarks_delay; break;
	case ACT_ROTATE:    actrotate(img); break;
	case ACT_DELETE:    actdelete(img); break;
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
	actdo(qacts[qact_ri].act,qacts[qact_ri].img);
	qact_ri=(qact_ri+1)%QACT_NUM;
	return 1;
}

int actthread(void *UNUSED(arg)){
	actcfg.savemarks_delay = cfggetuint("act.savemarks_delay");
	actcfg.runcmd          = cfggetbool("act.do");
	while(!sdl_quit){
		acttrysavemarks();
		if(!actpop()) SDL_Delay(500);
	}
	if(actcfg.savemarks) actsavemarks();
	sdl_quit|=THR_ACT;
	return 0;
}
