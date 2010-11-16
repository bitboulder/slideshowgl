#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <SDL.h>
#include <pthread.h>
#include "main.h"
#include "gl.h"
#include "sdl.h"
#include "load.h"
#include "dpl.h"
#include "cfg.h"

#ifdef __linux__
	#define SCHED_PRIORITY	__sched_priority
#elif defined __WIN32__
	#define SCHED_PRIORITY	sched_priority
#else
	#error "Unsupported platform"
#endif

enum debug dbg = DBG_NONE;
#define E(X)	#X
char *dbgstr[] = { EDEBUG };
#undef E

struct mainthread {
	void *(*fnc)(void *);
	int pri;
	char init;
	pthread_t pt;
} mainthreads[] = {
	{ &sdlthread, 15, 0 },
	{ &dplthread, 10, 0 },
	{ &ldthread,   5, 0 },
	{ NULL,        0, 0 },
};

void debug_ex(enum debug lvl,char *file,int line,char *txt,...){
	va_list ap;
	char err = lvl==ERR_QUIT || lvl==ERR_CONT;
	FILE *fout = err ? stderr : stdout;
	char fcp[5];
	char *pos;
	if(!err && lvl>dbg) return;
	if((pos=strrchr(file,'/'))) file=pos+1;
	snprintf(fcp,5,file);
	if((pos=strchr(fcp,'.'))) *pos='\0';
	fprintf(fout,"%-4s(%3i) ",fcp,line);
	if(err) fprintf(fout,"ERROR   : ");
	else    fprintf(fout,"dbg-%-4s: ",dbgstr[lvl]);
	va_start(ap,txt);
	vfprintf(fout,txt,ap);
	va_end(ap);
	fprintf(fout,"\n");
	if(lvl!=ERR_QUIT) return;
	SDL_Quit();
	exit(1);
}

void start_threads(){
	struct mainthread *mt=mainthreads;
	struct sched_param par;
	mainthreads->pt=pthread_self();
	mainthreads->init=1;
	for(;mt->fnc;mt++){
		if(!mt->init){
			pthread_create(&mt->pt,NULL,mt->fnc,NULL);
			mt->init=1;
		}
		par.SCHED_PRIORITY=mt->pri;
		pthread_setschedparam(mt->pt,SCHED_RR,&par);
	}
	mainthreads->fnc(NULL);
}

int main(int argc,char **argv){
	cfgparseargs(argc,argv);
	dbg=cfggetint("main.dbg");
	ldgetfiles(argc-optind,argv+optind);
	sdlinit();
	start_threads();
	return 0;
}
