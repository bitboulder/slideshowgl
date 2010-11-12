#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "load.h"
#include "sdl.h"

struct load load = {
	.nimg = 0,
	.imgs = NULL,
};

void ldcheck(){
}

void *ldthread(void *arg){
	while(!sdl.quit){
		ldcheck();
		SLEEP(100);
	}
	return NULL;
}

void ldaddimg(char *fn,int *simg){
	if(load.nimg==*simg) load.imgs=realloc(load.imgs,sizeof(struct img)*(*simg+=16));
	memset(load.imgs+load.nimg,0,sizeof(struct img));
	load.imgs[load.nimg].fn=fn;
	load.nimg++;
}

void ldgetfiles(int argc,char **argv){
	int simg=0;
	for(;argc;argc--,argv++){
		ldaddimg(argv[0],&simg);
	}
}
