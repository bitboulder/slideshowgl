#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <SDL.h>
#include "mapld.h"
#include "main.h"
#include "help.h"
#include "cfg.h"
#include "sdl.h"

#define N_TIS 8

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

void mapld_load(struct mapldti ti){
	char cmd[FILELEN*2];
	char fn[FILELEN];
	snprintf(fn,FILELEN,"%s/%s/%i/%i/%i_ld.png",mapld.cachedir,ti.maptype,ti.iz,ti.ix,ti.iy);
	snprintf(cmd,FILELEN*2,"mkdir -p %s/%s/%i/%i/",mapld.cachedir,ti.maptype,ti.iz,ti.ix);
	system(cmd);
	if(!strcmp(ti.maptype,"om")) snprintf(cmd,FILELEN*2,"wget -qO %s http://tile.openstreetmap.org/mapnik/%i/%i/%i.png",fn,ti.iz,ti.ix,ti.iy);
	system(cmd);
	if(filesize(fn)){
		snprintf(cmd,FILELEN,"%s/%s/%i/%i/%i.png",mapld.cachedir,ti.maptype,ti.iz,ti.ix,ti.iy);
		rename(fn,cmd);
	}else unlink(fn);
}

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
	mapld_load(ti);
	return 1;
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
