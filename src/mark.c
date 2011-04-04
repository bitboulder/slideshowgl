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
	struct {
		char fn[FILELEN];
		const char *flst2gthumb;
	} cfg;
	char init;
	struct mk *mks[MKCHAINS];
	char *catfn;
	char *catna;
	size_t ncat;
	char *mkchange;
} mark = {
	.init = 0,
	.catfn = NULL,
	.catna = NULL,
	.ncat = 0,
};

size_t markncat(){ return mark.ncat; }
char *markcats(){ return mark.catna; }
char *markcatfn(int id,const char **na){
	if(id<0 || (size_t)id>=mark.ncat) return NULL;
	if(na) *na=mark.catna+id*FILELEN;
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
	mark.catfn=realloc(mark.catfn,sizeof(char)*FILELEN*((++mark.ncat)+1));
	mark.catna=realloc(mark.catna,sizeof(char)*(FILELEN*mark.ncat+1));
	for(i=0;i<mark.ncat-1;i++) if(strncmp(fn,mark.catfn+i*FILELEN,FILELEN)<=0) break;
	if(i<mark.ncat-1){
		memmove(mark.catfn+(i+1)*FILELEN,mark.catfn+i*FILELEN,(mark.ncat-i-1)*FILELEN);
		memmove(mark.catna+(i+1)*FILELEN,mark.catna+i*FILELEN,(mark.ncat-i-1)*FILELEN);
	}
	mark.catfn[mark.ncat*FILELEN]='\0';
	mark.catna[mark.ncat*FILELEN]='\0';
	cfn=mark.catfn+FILELEN*i;
	cna=mark.catna+FILELEN*i;
	len=strlen(fn);
	if(len>FILELEN-1) len=FILELEN-1;
	memcpy(cfn,fn,len);
	cfn[len]='\0';
	for(i=len;i>0;i--) if(fn[i-1]=='/' || fn[i-1]=='\\') break;
	len-=i;
	memcpy(cna,fn+i,len);
	for(i=0;i<len;i++) if(cna[i]=='.') break;
	cna[i]='\0';
	utf8check(cna);
}

void markcatsel(char *catsel){
	size_t i;
	size_t len=strlen(catsel);
	char *na;
	if(len) for(i=0;i<mark.ncat;i++) if(!strncasecmp(catsel,na=mark.catna+i*FILELEN,len)){
		size_t l=strlen(na);
		memcpy(catsel,na,len);
		memcpy(catsel+len+1,na+len,l-len+1);
		*(int*)(catsel+l+2)=(int)i;
		return;
	}
	catsel[len+1]='\0';
	*(int*)(catsel+len+2)=-1;
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
		snprintf(mk->fn,FILELEN,fn);
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
		char *fn=c?mark.catfn+(c-1)*FILELEN:mark.cfg.fn;
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
	FILE *fd;
	if(mark.init) return;
	mark.cfg.flst2gthumb=cfggetstr("mark.flst2gthumb");
	if((fd=fopen(mark.cfg.flst2gthumb,"r"))) fclose(fd);
	else mark.cfg.flst2gthumb=NULL;
	fn=cfggetstr("mark.fn");
	if(fn && fn[0]) snprintf(mark.cfg.fn,FILELEN,fn);
	else{
		fn=getenv("TEMP");
		if(!fn) fn=getenv("TMP");
		if(!fn) fn="/tmp";
		snprintf(mark.cfg.fn,FILELEN,"%s/slideshowgl-mark.flst",fn);
	}
	mark.mkchange=calloc(mark.ncat+1,sizeof(char));
	memset(mark.mks,0,sizeof(struct mk *)*MKCHAINS);
	marksload();
	mark.init=1;
}

char *markimgget(struct img *img,enum mkcreate create){
	struct mk *mk;
	markinit();
	mk=mkfind(imgfilefn(img->file),create);
	return mk ? mk->mark : NULL;
}

void markchange(size_t id){ mark.mkchange[id]=1; }

void markssave(){
	FILE *fd;
	int i;
	size_t c;
	struct mk *mk;
	markinit();
	for(c=0;c<=mark.ncat;c++) if(mark.mkchange[c]){
		char *fn=c?mark.catfn+(c-1)*FILELEN:mark.cfg.fn;
		mark.mkchange[c]=0;
		if(!(fd=fopen(fn,"w"))) return;
		for(i=0;i<MKCHAINS;i++) for(mk=mark.mks[i];mk;mk=mk->nxt)
			if(mk->mark[c]) fprintf(fd,"%s\n",mk->fn);
		fclose(fd);
		if(mark.cfg.flst2gthumb){
			char buf[FILELEN*2];
			snprintf(buf,FILELEN*2,"%s \"%s\"",mark.cfg.flst2gthumb,fn);
			system(buf);
		}
		debug(DBG_STA,"marks saved (%s)",fn);
	}
}

