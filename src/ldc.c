#include <stdlib.h>
#include <stdio.h>
#include <SDL.h>
#include <SDL_image.h>
#include "ldc.h"
#include "main.h"
#include "ldjpg.h"

#define NLDC	8

struct ldc {
	struct sdlimg *sdlimg;
	char fn[FILELEN];
	char swap;
};
struct ldcache {
	struct ldc ldc[NLDC];
	int wi;
} ldcache;

void ldcinit(){
	memset(&ldcache,0,sizeof(struct ldcache));
}

void ldcput(struct sdlimg *sdlimg,const char *fn,char swap){
	struct ldc *p=ldcache.ldc+(ldcache.wi++);
	if(ldcache.wi==NLDC) ldcache.wi=0;
	if(p->sdlimg) sdlimg_unref(p->sdlimg);
	p->sdlimg=sdlimg;
	snprintf(p->fn,FILELEN,"%s",fn);
	p->swap=swap;
	sdlimg_ref(sdlimg);
}

struct ldc *ldcfind(const char *fn){
	struct ldc *p;
	int i;
	for(p=ldcache.ldc,i=0;i<NLDC;i++,p++) if(p->sdlimg && !strncmp(fn,p->fn,FILELEN)) return p;
	return NULL;
}

struct sdlimg *ldc(const char *fn,char force,char *swap){
	struct ldc *p;
	if(!force && (p=ldcfind(fn))){
		*swap=p->swap;
		sdlimg_ref(p->sdlimg);
		return p->sdlimg;
	}else{
		struct sdlimg *sdlimg=sdlimg_gen(IMG_Load(fn));
		if(!sdlimg){ *swap=1; sdlimg=sdlimg_gen(JPG_LoadSwap(fn)); }
		if(sdlimg->sf->w>1024 && sdlimg->sf->h>1024) ldcput(sdlimg,fn,*swap);
		return sdlimg;
	}
}
