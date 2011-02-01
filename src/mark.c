#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#if HAVE_OPENDIR
	#include <sys/types.h>
	#include <dirent.h>
	#ifndef NAME_MAX
		#define NAME_MAX 255
	#endif
#endif
#include "mark.h"
#include "img.h"
#include "main.h"
#include "cfg.h"
#include "file.h"
#include "eff.h"
#include "help.h"

struct mk {
	struct mk *nxt;
	char fn[FILELEN];
	char *cmp;
	char *mark;
};

struct mkcat {
	char fn[FILELEN];
	char name[FILELEN];
};

#define MKCHAINS	512

struct mark {
	char init;
	char fn[FILELEN];
	struct mk *mks[MKCHAINS];
	char *catfn;
	char *catna;
	size_t ncat;
} mark = {
	.init = 0,
	.catfn = NULL,
	.catna = NULL,
	.ncat = 0,
};

size_t markncat(){ return mark.ncat; }
char *markcats(){ return mark.catna; }
char *markcatfn(int id){
	if(id<0 || (size_t)id>=mark.ncat) return NULL;
	return mark.catfn+id*FILELEN;
}

void markcatadddir(char *fn){
#if HAVE_OPENDIR
	DIR *dd;
	struct dirent *de;
	char buf[FILELEN];
	size_t ld;
	if(!(dd=opendir(fn))) return;
	ld=strlen(fn);
	memcpy(buf,fn,ld);
	if(ld && buf[ld-1]!='/' && buf[ld-1]!='\\' && ld<FILELEN) buf[ld++]='/';
	while((de=readdir(dd))){
		size_t l=0;
		while(l<NAME_MAX && de->d_name[l]) l++;
		if(l<6 || strncmp(de->d_name+l-5,".flst",5)) continue;
		if(ld+l>=FILELEN) continue;
		memcpy(buf+ld,de->d_name,l);
		buf[ld+l]='\0';
		markcatadd(buf);
	}
	closedir(dd);
#endif
}

void markcatadd(char *fn){
	char *cfn;
	char *cna;
	size_t len;
	size_t i;
	if(mark.init) return;
	if(isdir(fn)==1){
		markcatadddir(fn);
		return;
	}
	mark.catfn=realloc(mark.catfn,sizeof(char)*FILELEN*(++mark.ncat));
	mark.catna=realloc(mark.catna,sizeof(char)*(FILELEN*mark.ncat+1));
	cfn=mark.catfn+FILELEN*(mark.ncat-1);
	cna=mark.catna+FILELEN*(mark.ncat-1);
	len=strlen(fn);
	if(len>FILELEN-1) len=FILELEN-1;
	memcpy(cfn,fn,len);
	cfn[len]='\0';
	for(i=len;i>0;i--) if(fn[i-1]=='/' || fn[i-1]=='\\') break;
	len-=i;
	memcpy(cna,fn+i,len);
	for(i=0;i<len;i++) if(cna[i]=='.') break;
	cna[i]='\0';
	cna[FILELEN]='\0';
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
	for(mk=mark.mks[hash];mk && strncmp(cmp,mk->cmp,FILELEN);) mk=mk->nxt;
	if(!mk && create>=MKC_YES){
		mk=malloc(sizeof(struct mk));
		strncpy(mk->fn,fn,FILELEN); mk->fn[FILELEN-1]='\0';
		mk->cmp=mk->fn+(cmp-fn);
		mk->nxt=mark.mks[hash];
		mark.mks[hash]=mk;
		mk->mark=calloc(mark.ncat+1,sizeof(char));
	}
	return mk;
}

void marksload(){
	FILE *fd;
	char line[FILELEN];
	size_t c;
	marksfree();
	for(c=0;c<=mark.ncat;c++){
		char *fn=c?mark.catfn+(c-1)*FILELEN:mark.fn;
		if(!(fd=fopen(fn,"r"))) continue;
		while(!feof(fd) && fgets(line,FILELEN,fd) && line[0]){
			int len=(int)strlen(line);
			while(len && (line[len-1]=='\n' || line[len-1]=='\r')) line[--len]='\0';
			mkfind(line,MKC_YES)->mark[c]=1;
		}
		fclose(fd);
		debug(DBG_STA,"marks loaded (%s)",fn);
	}
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
	size_t c;
	struct mk *mk;
	markinit();
	for(c=0;c<=mark.ncat;c++){
		char *fn=c?mark.catfn+(c-1)*FILELEN:mark.fn;
		if(!(fd=fopen(fn,"w"))) return;
		for(i=0;i<MKCHAINS;i++) for(mk=mark.mks[i];mk;mk=mk->nxt)
			if(mk->mark[c]) fprintf(fd,"%s\n",mk->fn);
		fclose(fd);
		debug(DBG_STA,"marks saved (%s)",fn);
	}
}

