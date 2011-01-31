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
	char *cmp;
	char *mark;
};

#define MKCHAINS	512

struct mark {
	char init;
	char fn[FILELEN];
	struct mk *mks[MKCHAINS];
	char *catalog;
	size_t ncatalog;
} mark = {
	.init = 0,
	.catalog = NULL,
	.ncatalog = 0,
};

void catalogadd(char *fn){
	if(mark.init) return;
	/* TODO */
}

const char *mkcmp(const char *fn){
	size_t c=0,i,len=strlen(fn);
	for(i=len;i>0 && c<3;i--) if(fn[i-1]=='/' || fn[i-1]=='\\') c++;
	if(i) i++;
	return fn+i;
}

int mkhash(const char *cmp){
	int hash=0;
	for(;cmp[0];cmp++) hash+=cmp[0];
	return abs(hash)%MKCHAINS;
}

void marksfree(){
	int i;
	for(i=0;i<MKCHAINS;i++) while(mark.mks[i]){
		struct mk *mkf=mark.mks[i];
		mark.mks[i]=mkf->nxt;
		free(mkf->mark);
		free(mkf);
	}
}

struct mk *mkfind(const char *fn,enum mkcreate create){
	const char *cmp=mkcmp(fn);
	int hash=mkhash(cmp);
	struct mk *mk=NULL;
	if(create<MKC_ALLWAYS) for(mk=mark.mks[hash];mk && strncmp(cmp,mk->cmp,FILELEN);) mk=mk->nxt;
	if(!mk && create>=MKC_YES){
		mk=malloc(sizeof(struct mk));
		strncpy(mk->fn,fn,FILELEN); mk->fn[FILELEN-1]='\0';
		mk->cmp=mk->fn+(cmp-fn);
		mk->nxt=mark.mks[hash];
		mark.mks[hash]=mk;
		mk->mark=calloc(mark.ncatalog+1,sizeof(char));
		mk->mark[0]=--create;
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

char *markimgget(struct img *img,enum mkcreate create){
	struct mk *mk;
	markinit();
	mk=mkfind(imgfilefn(img->file),create);
	return mk ? mk->mark : NULL;
}

void markssave(){
	FILE *fd;
	int i;
	struct mk *mk;
	markinit();
	if(!(fd=fopen(mark.fn,"w"))) return;
	for(i=0;i<MKCHAINS;i++) for(mk=mark.mks[i];mk;mk=mk->nxt)
		if(mk->mark[0]) fprintf(fd,"%s\n",mk->fn);
	fclose(fd);
	debug(DBG_STA,"marks saved (%s)",mark.fn);
}

