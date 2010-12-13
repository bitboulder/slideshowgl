#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <SDL.h>
#include <time.h>
#include "main.h"
#include "gl.h"
#include "sdl.h"
#include "load.h"
#include "dpl.h"
#include "cfg.h"
#include "act.h"
#include "file.h"

#if 0
#if SDL_THREAD_PTHREAD && HAVE_PTHREAD
#include <pthread.h>
typedef pthread_t SYS_ThreadHandle;
struct SDL_Thread {
	Uint32 threadid;
	SYS_ThreadHandle handle;
	int status;
	//SDL_error errbuf;
	//void *data;
};
#endif
#endif

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
	vfprintf(fout,_(txt),ap);
	va_end(ap);
	fprintf(fout,"\n");
	if(lvl!=ERR_QUIT) return;
	SDL_Quit();
	exit(1);
}

char *progpath=NULL;

char *finddatafile(char *fn){
	int i;
	static char ret[FILELEN];
	static char *dirs[]={NULL, NULL, ".", "data", "../data", DATADIR, NULL};
	if(progpath && !dirs[0]){
		for(i=strlen(progpath)-1;i>=0;i--) if(progpath[i]=='/' || progpath[i]=='\\'){
			dirs[0]=malloc(i+1);
			dirs[1]=malloc(i+6);
			memcpy(dirs[0],progpath,i);
			memcpy(dirs[1],progpath,i+1);
			dirs[0][i]='\0';
			strcpy(dirs[1]+i+1,"data");
			break;
		}
	}
	for(i=0;i<2 || dirs[i];i++) if(dirs[i]){
		FILE *fd;
		snprintf(ret,FILELEN,"%s/%s",dirs[i],fn);
		if(!(fd=fopen(ret,"rb"))) continue;
		fclose(fd);
		return ret;
	}
	error(ERR_CONT,"could not find data file: '%s'",fn);
	return NULL;
}

char *textload(char *fn){
	FILE *fd;
	long len;
	char *buf;
	if(!fn) return NULL;
	if(!(fd=fopen(fn,"r"))) return NULL;
	fseek(fd,0,SEEK_END);
	len=ftell(fd);
	fseek(fd,0,SEEK_SET);
	buf=malloc(len+1);
	fread(buf,1,len,fd);
	fclose(fd);
	buf[len]='\0';
	return buf;
}

struct mainthread {
	int (*fnc)(void *);
	int pri;
	char init;
	SDL_Thread *pt;
} mainthreads[] = {
	{ &sdlthread, 15, 0 },
	{ &dplthread, 10, 0 },
	{ &ldthread,   5, 0 },
	{ &actthread,  1, 0 },
	{ NULL,        0, 0 },
};

void start_threads(){
	struct mainthread *mt=mainthreads;
	mainthreads->pt=NULL;
	mainthreads->init=1;
	for(;mt->fnc;mt++) if(!mt->init){
		mt->pt=SDL_CreateThread(mt->fnc,NULL);
		mt->init=1;
	}
#if 0
#if SDL_THREAD_PTHREAD && HAVE_PTHREAD
	for(mt=mainthreads;mt->fnc;mt++){
		struct sched_param par;
		par.sched_priority=mt->pri;
		pthread_setschedparam(mt->pt ? mt->pt->handle : pthread_self(),SCHED_RR,&par);
	}
#endif
#endif
	mainthreads->fnc(NULL);
}

char end_threads(){
	int i;
	for(i=500;(sdl_quit&THR_OTH)!=THR_OTH && i>0;i--) SDL_Delay(10);
	return i;
}

int main(int argc,char **argv){
	if(argc) progpath=argv[0];
	srand((unsigned int)time(NULL));
	cfgparseargs(argc,argv);
	dbg=cfggetint("main.dbg");
	tim=cfggetenum("main.timer");
	fgetfiles(argc-optind,argv+optind);
	sdlinit();
	start_threads();
	if(!end_threads())
		error(ERR_CONT,"sdl timeout waiting for threads:%s%s%s",
			(sdl_quit&THR_SDL)?"":" sdl",
			(sdl_quit&THR_DPL)?"":" dpl",
			(sdl_quit&THR_LD )?"":" ld",
			(sdl_quit&THR_ACT)?"":" act");
	else sdlquit();
	return 0;
}
