#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if HAVE_ZLIB
	#include <zlib.h>
#else
	#define gzFile			FILE*
	#define gzopen			fopen
	#define	gzclose			fclose
	#define gzeof			feof
	#define gzprintf		fprintf
	#define gzgets(f,b,l)	fgets(b,l,f)
#endif
#include "exifch.h"
#include "exif_int.h"
#include "main.h"
#include "cfg.h"
#include "help.h"
#include "act.h"

#define ECCHAINS	5120
struct exifcachei {
	struct exifcachei *nxt;
	char fn[FILELEN];
	long ft;
	int64_t date;
};
struct exifcache {
	char fn[FILELEN];
	struct exifcachei *ci[ECCHAINS];
} exifcache = {
	.fn = { '\0' },
};



unsigned int exifcachehkey(const char *fn){
	unsigned hkey=0;
	int i;
	for(i=0;fn[i];i++) hkey+=(unsigned int)fn[i];
	return hkey%ECCHAINS;
}

struct exifcachei *exifcachefind(const char *fn){
	struct exifcachei *cache=exifcache.ci[exifcachehkey(fn)];
	for(;cache;cache=cache->nxt) if(!strncmp(fn,cache->fn,FILELEN)) return cache;
	return NULL;
}

/* thread: dpl */
char exifcachecheck(struct imgexif *exif,const char *fn){
	struct exifcachei *cache=exifcachefind(fn);
	if(!cache) return 0;
	if(filetime(fn)>cache->ft) return 0;
	exif->date=cache->date;
	return 1;
}

/* thread: load,dpl */
void exifcacheadd(struct imgexif *exif,const char *fn){
	struct exifcachei *ci=exifcachefind(fn);
	if(!ci){
		unsigned int hkey=exifcachehkey(fn);
		ci=malloc(sizeof(struct exifcachei));
		snprintf(ci->fn,FILELEN,"%s",fn);
		ci->ft=filetime(fn);
		ci->nxt=exifcache.ci[hkey];
		exifcache.ci[hkey]=ci;
	}else if(filetime(fn)<=ci->ft) return;
	ci->date=exif->date;
	actadd(ACT_EXIFCACHE,NULL,NULL);
}

void exifcachegetfile(){
	const char *cf;
	if(exifcache.fn[0]) return;
	memset(exifcache.ci,0,sizeof(struct exifcachei *)*ECCHAINS);
	cf=cfggetstr("exif.cachefile");
	if(cf && cf[0]) snprintf(exifcache.fn,FILELEN,cf);
	else snprintf(exifcache.fn,FILELEN,"%s/slideshowgl-cache/exif.gz",gettmp());
	/* TODO: mkdirm(exifcache.cachefile,1);  */
}

/* thread: load */
void exifcacheload(){
	gzFile fd;
	exifcachegetfile();
	if(!(fd=gzopen(exifcache.fn,"rb"))) return;
	memset(exifcache.ci,0,sizeof(struct exifcachei *)*ECCHAINS);
	while(!gzeof(fd)){
		struct exifcachei cr,*cm;
		char buf[FILELEN*2];
		unsigned int hkey;
		if(!(gzgets(fd,buf,FILELEN*2-1))) continue;
		if(sscanf(buf,"%li %li %s\n",&cr.ft,&cr.date,cr.fn)!=3) continue;
		cm=malloc(sizeof(struct exifcachei));
		memcpy(cm,&cr,sizeof(struct exifcachei));
		hkey=exifcachehkey(cr.fn);
		cm->nxt=exifcache.ci[hkey];
		exifcache.ci[hkey]=cm;
	}
	gzclose(fd);
}

/* thread: act */
void exifcachesave(){
	struct exifcachei *ci;
	unsigned int hkey;
	gzFile fd;
	exifcachegetfile();
	if(!(fd=gzopen(exifcache.fn,"wb"))) return;
	for(hkey=0;hkey<ECCHAINS;hkey++)
		for(ci=exifcache.ci[hkey];ci;ci=ci->nxt)
			gzprintf(fd,"%li %li %s\n",ci->ft,ci->date,ci->fn);
	gzclose(fd);
}

