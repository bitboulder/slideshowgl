#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <SDL.h>
#if HAVE_CURL
	#include <curl/curl.h>
#endif
#include "mapld.h"
#include "main.h"
#include "help.h"
#include "cfg.h"
#include "sdl.h"

#define N_TIS 4

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

void mapld_load(struct mapldti ti){
	char url[FILELEN*2]={'\0'},*txt=url;
	char fn[FILELEN];
	FILE *fd;
	CURL *curl;
	CURLcode res;
	int i;
	for(i=0;i<3;i++){
		switch(i){
		case 0: snprintf(txt,(size_t)(txt+FILELEN*2-url),"%s/%s",mapld.cachedir,ti.maptype); break;
		case 1: snprintf(txt,(size_t)(txt+FILELEN*2-url),"/%i",ti.iz); break;
		case 2: snprintf(txt,(size_t)(txt+FILELEN*2-url),"/%i",ti.ix); break;
		}
		txt+=strlen(txt);
		mkdirm(url);
	}
	snprintf(fn,FILELEN,"%s/%s/%i/%i/%i_ld.png",mapld.cachedir,ti.maptype,ti.iz,ti.ix,ti.iy);
	if(!strcmp(ti.maptype,"om")) snprintf(url,FILELEN*2,"http://tile.openstreetmap.org/mapnik/%i/%i/%i.png",ti.iz,ti.ix,ti.iy);
	else{ error(ERR_CONT,"mapld_load: unknown maptype \"%s\"",ti.maptype); return; }
	curl=curl_easy_init();
	if(!curl) return;
	if(!(fd=fopen(fn,"w"))){ curl_easy_cleanup(curl); return; }
	curl_easy_setopt(curl,CURLOPT_URL,url);
	curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,mapld_load_data);
	curl_easy_setopt(curl,CURLOPT_WRITEDATA,&fd);
	res=curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	fclose(fd);
	if(res==CURLE_OK && filesize(fn)){
		snprintf(url,FILELEN,"%s/%s/%i/%i/%i.png",mapld.cachedir,ti.maptype,ti.iz,ti.ix,ti.iy);
		rename(fn,url);
	}else unlink(fn);
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

void mapld_put(const char *maptype,int iz,int ix,int iy){
	int nwi=(mapld.wi+1)%N_TIS;
	if(nwi==mapld.ri) return;
	mapld.tis[mapld.wi].maptype=maptype;
	mapld.tis[mapld.wi].iz=iz;
	mapld.tis[mapld.wi].ix=ix;
	mapld.tis[mapld.wi].iy=iy;
	mapld.wi=nwi;
}

char mapld_check(const char *maptype,int iz,int ix,int iy,char *fn){
	if(!mapld.init) return 0;
	snprintf(fn,FILELEN,"%s/%s/%i/%i/%i.png",mapld.cachedir,maptype,iz,ix,iy);
	if(isfile(fn)) return 1;
	mapld_put(maptype,iz,ix,iy);
	return 0;
}

void mapldinit(){
	const char *cd;
	mapld.mutex=SDL_CreateMutex();
	cd=cfggetstr("mapld.cachedir");
	if(cd && cd[0]) snprintf(mapld.cachedir,FILELEN,cd);
	else snprintf(mapld.cachedir,FILELEN,"%s/slideshowgl-cache",gettmp());
	mkdirm(mapld.cachedir);
	mapld.init=1;
}

int mapldthread(void *arg){
	long id=(long)arg;
	if(!id) mapldinit();
	else while(!mapld.init) SDL_Delay(500);
	while(!sdl_quit){
		if(!mapld_get()) SDL_Delay(250);
	}
	switch(id){
	case 0: sdl_quit|=THR_ML1; break;
	case 1: sdl_quit|=THR_ML2; break;
	}
	return 0;
}
