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

#define N_TIS 6

struct mapld {
	int wi,ri;
	struct mapldti {
		const char *maptype;
		int iz,ix,iy;
	} tis[N_TIS];
	char cachedir[FILELEN];
	char init;
	SDL_mutex *mutex;
} mapld = {
	.wi=0,
	.ri=0,
	.init=0,
};

#if HAVE_CURL
static size_t mapld_load_data(void *data,size_t size,size_t nmemb,void *arg){
	FILE **fd=(FILE**)arg;
	return fwrite(data,size,nmemb,*fd);
}

char mapld_filecheck(const char *fn){
	FILE *fd=fopen(fn,"rb");
	char buf[8];
	if(!fd) return 0;
	if(fread(buf,1,8,fd)!=8){ fclose(fd); return 0; }
	fclose(fd);
	if(!strncmp(buf,"<html>",6)) return 0;
	return 1;
}

void mapld_load(struct mapldti ti){
	char url[FILELEN*2];
	char fn[FILELEN];
	FILE *fd;
	CURL *curl;
	CURLcode res;
	struct curl_slist *lst=NULL;
	snprintf(fn,FILELEN,"%s/%s/%i/%i/%i_ld.png",mapld.cachedir,ti.maptype,ti.iz,ti.ix,ti.iy);
	mkdirm(fn,1);
	if(!strcmp(ti.maptype,"om")) snprintf(url,FILELEN*2,"http://tile.openstreetmap.org/mapnik/%i/%i/%i.png",ti.iz,ti.ix,ti.iy);
	else{ error(ERR_CONT,"mapld_load: unknown maptype \"%s\"",ti.maptype); return; }
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
		snprintf(url,FILELEN,"%s/%s/%i/%i/%i.png",mapld.cachedir,ti.maptype,ti.iz,ti.ix,ti.iy);
		rename(fn,url);
	}else{
		error(ERR_CONT,"mapld_load: file loading failed (res: %i)",res);
		unlink(fn);
	}
}
#endif

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

char mapld_put(const char *maptype,int iz,int ix,int iy){
	int nwi=(mapld.wi+1)%N_TIS;
	if(nwi==mapld.ri) return 0;
	mapld.tis[mapld.wi].maptype=maptype;
	mapld.tis[mapld.wi].iz=iz;
	mapld.tis[mapld.wi].ix=ix;
	mapld.tis[mapld.wi].iy=iy;
	mapld.wi=nwi;
	return 1;
}

char mapld_check(const char *maptype,int iz,int ix,int iy,char web,char *fn){
	enum filetype ft;
	if(!mapld.init) return 0;
	snprintf(fn,FILELEN,"%s/%s/%i/%i/%i.png",mapld.cachedir,maptype,iz,ix,iy);
	ft=filetype(fn);
	if(ft&FT_FILE) return 2;
	if(ft!=FT_NX) return 1;
	if(!web) return 1;
	return mapld_put(maptype,iz,ix,iy);
}

void mapldinit(){
	const char *cd;
	mapld.mutex=SDL_CreateMutex();
	cd=cfggetstr("mapld.cachedir");
	if(cd && cd[0]) snprintf(mapld.cachedir,FILELEN,cd);
	else snprintf(mapld.cachedir,FILELEN,"%s/slideshowgl-cache",gettmp());
	mkdirm(mapld.cachedir,0);
#if HAVE_CURL
	if(curl_global_init(CURL_GLOBAL_NOTHING)) return;
#endif
	mapld.init=1;
}

int mapldthread(void *arg){
	long id=(long)arg;
	if(!mapld.init) return 0;
	while(!sdl_quit){
		if(!mapld_get()) SDL_Delay(250);
	}
	switch(id){
	case 0: sdl_quit|=THR_ML1; break;
	case 1: sdl_quit|=THR_ML2; break;
	}
	return 0;
}
