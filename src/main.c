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
#include "act.h"

enum timer tim;
#define TIMER_NUM	16
void timer(enum timer timer,int id,char reset){
	static Uint32 timer_max[TIMER_NUM];
	static Uint32 timer_sum[TIMER_NUM];
	static Uint32 timer_cnt[TIMER_NUM];
	static Uint32 last=0, lastp=0;
	Uint32 now=SDL_GetTicks();
	if(timer!=tim) return;
	if(id>=0 && id<TIMER_NUM && last){
		Uint32 diff=now-last;
		if(timer_max[id]<diff) timer_max[id]=diff;
		timer_sum[id]+=diff;
		timer_cnt[id]++;
	}
	last=now;
	if(now-lastp>2000){
		if(lastp){
			int i,l;
			char tmp[256];
			snprintf(tmp,256,"timer:");
			for(l=TIMER_NUM-1;l>=0 && !timer_cnt[l];) l--;
			for(i=0;i<=l;i++) snprintf(tmp+strlen(tmp),256-strlen(tmp)," %6.1f(%4i)",
					(float)timer_sum[i]/(float)timer_cnt[i],timer_max[i]);
			debug(DBG_NONE,tmp);
		}
		if(reset){
			memset(timer_max,0,sizeof(Uint32)*TIMER_NUM);
			memset(timer_sum,0,sizeof(Uint32)*TIMER_NUM);
			memset(timer_cnt,0,sizeof(Uint32)*TIMER_NUM);
		}
		lastp=now;
	}
}

enum debug dbg = DBG_NONE;
#define E(X)	#X
char *dbgstr[] = { EDEBUG };
#undef E

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

#ifdef __linux__
	#define SCHED_PRIORITY	__sched_priority
#elif defined __WIN32__
	#define SCHED_PRIORITY	sched_priority
#else
	#error "Unsupported platform"
#endif

struct mainthread {
	void *(*fnc)(void *);
	int pri;
	char init;
	pthread_t pt;
} mainthreads[] = {
	{ &sdlthread, 15, 0 },
	{ &dplthread, 10, 0 },
	{ &ldthread,   5, 0 },
	{ &actthread,  1, 0 },
	{ NULL,        0, 0 },
};

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
	srand((unsigned int)time(NULL));
	cfgparseargs(argc,argv);
	dbg=cfggetint("main.dbg");
	tim=cfggetint("main.timer");
	ldgetfiles(argc-optind,argv+optind);
	sdlinit();
	start_threads();
	return 0;
}
