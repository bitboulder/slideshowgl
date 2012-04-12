#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zlib.h"
#include "exich.h"
#include "exif_int.h"
#include "main.h"
#include "cfg.h"
#include "help.h"
#include "act.h"

#define ECCHAINS	5120
struct exichi {
	struct exichi *nxt;
	char fn[FILELEN];
	long ft;
	int64_t date;
};
struct exich {
	char fn[FILELEN];
	struct exichi *ci[ECCHAINS];
} exich = {
	.fn = { '\0' },
};



unsigned int exichhkey(const char *cmp){
	unsigned hkey=0;
	int i;
	for(i=0;cmp[i];i++) hkey+=(unsigned int)cmp[i];
	return hkey%ECCHAINS;
}

struct exichi *exichfind(const char *fn){
	const char *cmp=fncmp(fn);
	struct exichi *cache=exich.ci[exichhkey(cmp)];
	for(;cache;cache=cache->nxt) if(!strncmp(cmp,cache->fn,FILELEN)) return cache;
	return NULL;
}

/* thread: dpl */
char exichcheck(struct imgexif *exif,const char *fn){
	struct exichi *cache=exichfind(fn);
	if(!cache) return 0;
	if(filetime(fn)>cache->ft) return 0;
	exif->date=cache->date;
	return 1;
}

/* thread: load,dpl */
void exichadd(struct imgexif *exif,const char *fn){
	struct exichi *ci=exichfind(fn);
	if(!ci){
		const char *cmp=fncmp(fn);
		unsigned int hkey=exichhkey(cmp);
		ci=malloc(sizeof(struct exichi));
		snprintf(ci->fn,FILELEN,"%s",cmp);
		ci->ft=filetime(fn);
		ci->nxt=exich.ci[hkey];
		exich.ci[hkey]=ci;
	}else if(filetime(fn)<=ci->ft) return;
	ci->date=exif->date;
	actadd(ACT_EXIFCACHE,NULL,NULL);
}

void exichgetfile(){
	const char *cf;
	if(exich.fn[0]) return;
	memset(exich.ci,0,sizeof(struct exichi *)*ECCHAINS);
	cf=cfggetstr("exif.cachefile");
	if(cf && cf[0]) snprintf(exich.fn,FILELEN,cf);
	else snprintf(exich.fn,FILELEN,"%s/slideshowgl-cache/exif.gz",gettmp());
	mkdirm(exich.fn,1);
}

/* thread: load */
void exichload(){
	gzFile fd;
	exichgetfile();
	if(!(fd=gzopen(exich.fn,"rb"))) return;
	memset(exich.ci,0,sizeof(struct exichi *)*ECCHAINS);
	while(!gzeof(fd)){
		struct exichi cr,*cm;
		char buf[FILELEN*2];
		unsigned int hkey;
		long t;
		if(!(gzgets(fd,buf,FILELEN*2-1))) continue;
		if(sscanf(buf,"%li %li %s\n",&cr.ft,&t,cr.fn)!=3) continue;
		cr.date=t;
		cm=malloc(sizeof(struct exichi));
		memcpy(cm,&cr,sizeof(struct exichi));
		hkey=exichhkey(cr.fn);
		cm->nxt=exich.ci[hkey];
		exich.ci[hkey]=cm;
	}
	gzclose(fd);
}

/* thread: act */
void exichsave(){
	struct exichi *ci;
	unsigned int hkey;
	gzFile fd;
	exichgetfile();
	if(!(fd=gzopen(exich.fn,"wb"))) return;
	for(hkey=0;hkey<ECCHAINS;hkey++)
		for(ci=exich.ci[hkey];ci;ci=ci->nxt)
			gzprintf(fd,"%li %li %s\n",ci->ft,ci->date,ci->fn);
	gzclose(fd);
}

