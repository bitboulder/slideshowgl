#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <SDL.h>
#if HAVE_CURL
	#undef DATADIR
	#include <curl/curl.h>
#endif
#include "mapld.h"
#include "main.h"
#include "help.h"
#include "cfg.h"
#include "sdl.h"
#include "map.h"
#include "map_int.h"

#define N_TIS 6

struct mapld {
	int wi,ri;
	struct mapldti {
		unsigned int mt;
		int iz,ix,iy;
	} tis[N_TIS];
	char cachedir[FILELEN];
	char init;
	SDL_mutex *mutex;
	int expire;
} mapld = {
	.wi=0,
	.ri=0,
	.init=0,
	.expire=7,
};

#if HAVE_CURL
static size_t mapld_load_data(void *data,size_t size,size_t nmemb,void *arg){
	FILE **fd=(FILE**)arg;
	return fwrite(data,size,nmemb,*fd);
}

char mapld_filecheck(const char *fn){
	FILE *fd=fopen(fn,"rb");
	char buf[8];
	size_t rd;
	if(!fd) return 0;
	rd=fread(buf,1,8,fd);
	fclose(fd);
	if(rd!=8 || !strncmp(buf,"<html>",6) || !strncmp(buf,"<!DOC",5)) return 0;
	return 1;
}

#define MURL	64

char mapld_parseurl(char **q,char *a,int x,int y,int z){
	if(**q>='0' && **q<='9'){ snprintf(a,MURL,"%i",**q-'0'); (*q)++; return 1; }
	switch(**q){
	case 'x': snprintf(a,MURL,"%i",x); (*q)++; return 1;
	case 'y': snprintf(a,MURL,"%i",y); (*q)++; return 1;
	case 'z': snprintf(a,MURL,"%i",z); (*q)++; return 1;
	case '"': {
		char *e=strchr(*q+1,'"');
		if(!e) return 0;
		snprintf(a,(size_t)MIN(MURL,(e-*q)),"%s",*q+1);
		*q=e+1;
		return 1;
	}
	case '+': case '*': case '%': {
		char o=**q,b[MURL],c[MURL];
		int ai=0,bi,ci;
		(*q)++;
		if(!mapld_parseurl(q,b,x,y,z)) return 0;
		if(!mapld_parseurl(q,c,x,y,z)) return 0;
		bi=atoi(b); ci=atoi(c);
		switch(o){
		case '+': ai=bi+ci; break;
		case '*': ai=bi*ci; break;
		case '%': ai=bi%ci; break;
		}
		snprintf(a,MURL,"%i",ai);
		return 1;
	}
	case 's': {
		char b[MURL],c[MURL],d[MURL];
		int ci,di;
		(*q)++;
		if(!mapld_parseurl(q,b,x,y,z)) return 0;
		if(!mapld_parseurl(q,c,x,y,z)) return 0;
		if(!mapld_parseurl(q,d,x,y,z)) return 0;
		ci=atoi(c); di=atoi(d);
		snprintf(a,(size_t)MIN(MIN(MURL,MURL-ci),di+1),"%s",b+ci);
		return 1;
	}
	}
	return 0;
}

char mapld_cplurl(char *url,const char *in,int x,int y,int z){
	char *p=url;
	int lu;
	snprintf(url,FILELEN*2,"%s",in);
	lu=(int)strlen(url);
	while((p=strchr(p,'['))){
		char *q=p+1,a[MURL];
		int la;
		if(!mapld_parseurl(&q,a,x,y,z) || *q!=']') return 0;
		la=(int)strlen(a);
		memmove(p+la,q+1,(size_t)MIN(FILELEN*2-(p-url)-la-1,lu-(q-url)));
		memcpy(p,a,(size_t)la);
		lu+=la-(int)(q-p)-1;
		p+=la;
	}
	url[FILELEN*2-1]='\0';
	return 1;
}

void mapld_load(struct mapldti ti){
	char url[FILELEN*2];
	char fn[FILELEN];
	FILE *fd;
	CURL *curl;
	CURLcode res;
	struct curl_slist *lst=NULL;
	snprintf(fn,FILELEN,"%s/%s/%i/%i/%i_ld.png",mapld.cachedir,maptypes[ti.mt].id,ti.iz,ti.ix,ti.iy);
	mkdirm(fn,1);
	if(!mapld_cplurl(url,maptypes[ti.mt].url,ti.ix,ti.iy,ti.iz))
	{ error(ERR_CONT,"mapld_cplurl failed: \"%s\"",maptypes[ti.mt].url); return; }
	debug(DBG_STA,"mapld_load: \"%s\" => \"%s\"",url,fn);
	curl=curl_easy_init();
	if(!curl){ error(ERR_CONT,"mapld_load: curl-init failed"); return; }
	if(!(fd=fopen(fn,"wb"))){
		error(ERR_CONT,"mapld_load: cache-file open failed (%s)",fn);
		curl_easy_cleanup(curl);
		return;
	}
	curl_easy_setopt(curl,CURLOPT_URL,url);
	curl_easy_setopt(curl,CURLOPT_USERAGENT,"Opera/9.80 (X11; Linux x86_64; U; de) Presto/2.9.168 Version/11.50");
	lst=curl_slist_append(lst,"Accept-Language: de");
	lst=curl_slist_append(lst,"Accept: image/png, image/jpeg, image/gif, */*");
	curl_easy_setopt(curl,CURLOPT_HTTPHEADER,lst);
	curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,mapld_load_data);
	curl_easy_setopt(curl,CURLOPT_WRITEDATA,&fd);
	res=curl_easy_perform(curl);
	curl_slist_free_all(lst);
	curl_easy_cleanup(curl);
	fclose(fd);
	if(res==CURLE_OK && filesize(fn) && mapld_filecheck(fn)){
		snprintf(url,FILELEN,"%s/%s/%i/%i/%i.png",mapld.cachedir,maptypes[ti.mt].id,ti.iz,ti.ix,ti.iy);
		rename(fn,url);
	}else{
		error(ERR_CONT,"mapld_load: file loading failed (res: %i)",res);
		unlink(fn);
	}
}
#endif

#ifndef MAPLDTEST
char mapld_get(){
	struct mapldti ti;
	if(SDL_LockMutex(mapld.mutex)==-1) return 0;
	if(mapld.wi==mapld.ri){
		SDL_UnlockMutex(mapld.mutex);
		return 0;
	}
	ti=mapld.tis[mapld.ri];
	mapld.ri=(mapld.ri+1)%N_TIS;
	SDL_UnlockMutex(mapld.mutex);
#if HAVE_CURL
	mapld_load(ti);
	return 1;
#else
	return 0;
#endif
}

char mapld_put(unsigned int mt,int iz,int ix,int iy){
	int nwi=(mapld.wi+1)%N_TIS;
	if(nwi==mapld.ri) return 0;
	mapld.tis[mapld.wi].mt=mt;
	mapld.tis[mapld.wi].iz=iz;
	mapld.tis[mapld.wi].ix=ix;
	mapld.tis[mapld.wi].iy=iy;
	mapld.wi=nwi;
	return 1;
}

char mapld_check(unsigned int mt,int iz,int ix,int iy,char web,char *fn){
	enum filetype ft;
	if(!mapld.init) return 0;
	snprintf(fn,FILELEN,"%s/%s/%i/%i/%i.png",mapld.cachedir,maptypes[mt].id,iz,ix,iy);
	ft=filetype(fn);
	if(ft&FT_FILE){
		if(filetime(fn)+mapld.expire*24*60*60<time(NULL)) mapld_put(mt,iz,ix,iy);
		return 2;
	}
	if(ft!=FT_NX) return 1;
	if(!web) return 1;
	return mapld_put(mt,iz,ix,iy);
}

void mapldinit(){
	const char *cd;
	mapld.mutex=SDL_CreateMutex();
	cd=cfggetstr("mapld.cachedir");
	if(cd && cd[0]) snprintf(mapld.cachedir,FILELEN,cd);
	else snprintf(mapld.cachedir,FILELEN,"%s/slideshowgl-cache",gettmp());
	mkdirm(mapld.cachedir,0);
	mapld.expire=cfggetint("mapld.expire");
#if HAVE_CURL
	if(curl_global_init(CURL_GLOBAL_NOTHING)) return;
#endif
	mapld.init=1;
}

int mapldthread(void *arg){
	long id=(long)arg;
	if(!mapld.init) return 0;
	while(!sdl_quit){
		char done=0;
		if(!id && maploadclt()) done=1;
		if(mapld_get()) done=1;
		if(!done) SDL_Delay(250);
	}
	switch(id){
	case 0: sdl_quit|=THR_ML1; break;
	case 1: sdl_quit|=THR_ML2; break;
	}
	return 0;
}
#endif
