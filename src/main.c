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

void usage(char *fn){
	printf("Usage: %s [-h]\n",fn);
}

void parse_args(int argc,char **argv){
	int c;
	while((c=getopt(argc,argv,"hvf"))>=0) switch(c){
	case 'h': usage(argv[0]); break;
	case 'v': dbg++; break;
	case 'f': cfgsetint("sdl.fullscreen",!cfggetint("sd.fullscreen")); break;
	}
	ldgetfiles(argc-optind,argv+optind);
}

/*#include <exif-data.h>
	ExifData *exif=exif_data_new_from_file("test.jpg");*/
int main(int argc,char **argv){
	parse_args(argc,argv);
	start_threads();
	return 0;
}
