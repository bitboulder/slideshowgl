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

struct mainthread {
	void *(*fnc)(void *);
	int pri;
	pthread_t pt;
} mainthreads[] = {
	{ &sdlthread, 15 },
	{ &dplthread, 10 },
	{ &ldthread,   5 },
	{ NULL,        0 },
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

void start_threads(){
	struct mainthread *mt=mainthreads;
	struct sched_param par;
	for(;mt->fnc;mt++){
		pthread_create(&mt->pt,NULL,mt->fnc,NULL);
		par.__sched_priority=mt->pri;
		pthread_setschedparam(mt->pt,SCHED_RR,&par);
	}
}

void usage(char *fn){
	printf("Usage: %s [-h]\n",fn);
}

void parse_args(int argc,char **argv){
	int c;
	while((c=getopt(argc,argv,"h"))>=0) switch(c){
	case 'h': usage(argv[0]); break;
	}
	ldgetfiles(argc-optind,argv+optind);
}

int main(int argc,char **argv){
	parse_args(argc,argv);
	start_threads();
	pthread_join(mainthreads->pt,NULL);
	return 0;
}
