#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mark.h"
#include "img.h"
#include "main.h"
#include "cfg.h"
#include "file.h"
#include "eff.h"

struct mk {
	struct mk *nxt;
	char fn[FILELEN];
	char mark;
};

#define MKCHAINS	512

struct mark {
	char init;
	char fn[FILELEN];
	struct mk *mks[MKCHAINS];
} mark = {
	.init = 0,
};

int mkhash(const char *line){
	int hash=0;
	for(;line[0];line++) hash+=line[0];
	return abs(hash)%MKCHAINS;
}

void marksfree(){
	int i;
	for(i=0;i<MKCHAINS;i++) while(mark.mks[i]){
		struct mk *mkf=mark.mks[i];
		mark.mks[i]=mkf->nxt;
		free(mkf);
	}
}

struct mk *mkfind(const char *fn,char create){
	int hash=mkhash(fn);
	struct mk *mk=NULL;
	if(create<2) for(mk=mark.mks[hash];mk && strncmp(fn,mk->fn,FILELEN);) mk=mk->nxt;
	if(!mk && create){
		mk=malloc(sizeof(struct mk));
		strncpy(mk->fn,fn,FILELEN); mk->fn[FILELEN-1]='\0';
		mk->nxt=mark.mks[hash];
		mark.mks[hash]=mk;
		mk->mark=1;
	}
	return mk;
}

void marksload(){
	FILE *fd;
	char line[FILELEN];
	if(!(fd=fopen(mark.fn,"r"))) return;
	marksfree();
	while(!feof(fd) && fgets(line,FILELEN,fd) && line[0]){
		int len=(int)strlen(line);
		while(len && (line[len-1]=='\n' || line[len-1]=='\r')) line[--len]='\0';
		mkfind(line,2);
	}
	fclose(fd);
	debug(DBG_STA,"marks loaded (%s)",mark.fn);
}

void markinit(){
	const char *fn;
	if(mark.init) return;
	memset(mark.mks,0,sizeof(struct mk *)*MKCHAINS);
	fn=cfggetstr("mark.fn");
	if(fn && fn[0]) snprintf(mark.fn,FILELEN,fn);
	else{
		fn=getenv("TEMP");
		if(!fn) fn=getenv("TMP");
		if(!fn) fn="/tmp";
		snprintf(mark.fn,FILELEN,"%s/slideshowgl-mark.flst",fn);
	}
	marksload();
	mark.init=1;
}

void markimgload(struct img *img){
	struct mk *mk;
	markinit();
	mk=mkfind(imgfilefn(img->file),0);
	*imgposmark(img->pos) = mk && mk->mark;
}

void markimgsave(struct img *img){
	char imk=*imgposmark(img->pos);
	struct mk *mk=mkfind(imgfilefn(img->file),imk);
	if(mk) mk->mark=imk;
}

void markssave(){
	FILE *fd;
	int i;
	struct mk *mk;
	markinit();
	ilforallimgs(markimgsave);
	if(!(fd=fopen(mark.fn,"w"))) return;
	for(i=0;i<MKCHAINS;i++) for(mk=mark.mks[i];mk;mk=mk->nxt)
		if(mk->mark) fprintf(fd,"%s\n",mk->fn);
	fclose(fd);
	debug(DBG_STA,"marks saved (%s)",mark.fn);
}

