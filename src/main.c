#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <SDL.h>
#include <pthread.h>
#include "main.h"
#include "gl.h"
#include "sdl.h"
#include "load.h"
#include "dpl.h"
#include "cfg.h"

enum debug dbg = DBG_STA;

struct mainthread {
	void *(*fnc)(void *);
	int pri;
	pthread_t pt;
} mainthreads[] = {
	{ &sdlthread, 15, 0 },
	{ &dplthread, 10, 0 },
	{ &ldthread,   5, 0 },
	{ NULL,        0, 0 },
};

void error(char quit,char *txt,...){
	int lfmt=strlen(txt)+9;
	char *fmt=malloc(lfmt);
	va_list ap;
	snprintf(fmt,lfmt,"ERROR: %s\n",txt);
	va_start(ap,txt);
	vfprintf(stderr,fmt,ap);
	va_end(ap);
	free(fmt);
	if(!quit) return;
	SDL_Quit();
	exit(1);
}

void debug(enum debug lvl,char *txt,...){
	va_list ap;
	if(lvl>dbg) return;
	printf("dbg: ");
	va_start(ap,txt);
	vprintf(txt,ap);
	va_end(ap);
	printf("\n");
}

void start_threads(){
	struct mainthread *mt=mainthreads;
	struct sched_param par;
	mainthreads->pt=pthread_self();
	for(;mt->fnc;mt++){
		if(!mt->pt) pthread_create(&mt->pt,NULL,mt->fnc,NULL);
		par.__sched_priority=mt->pri;
		pthread_setschedparam(mt->pt,SCHED_RR,&par);
	}
	mainthreads->fnc(NULL);
}

void usage(char *fn){
	printf("Usage: %s [-h]\n",fn);
}

void parse_args(int argc,char **argv){
	int c;
	while((c=getopt(argc,argv,"hf"))>=0) switch(c){
	case 'h': usage(argv[0]); break;
	case 'f': cfgsetint("sdl.fullscreen",!cfggetint("sd.fullscreen")); break;
	}
	ldgetfiles(argc-optind,argv+optind);
}

int main(int argc,char **argv){
	parse_args(argc,argv);
	start_threads();
	return 0;
}
