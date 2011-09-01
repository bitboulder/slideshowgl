#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <SDL.h>
#include <time.h>
#ifndef __WIN32__
	#include <sys/types.h>
	#include <sys/wait.h>
	#include <sys/time.h>
	#include <sys/resource.h>
#endif
#include "main.h"
#include "gl.h"
#include "sdl.h"
#include "load.h"
#include "dpl.h"
#include "cfg.h"
#include "act.h"
#include "file.h"
#include "mapld.h"
#include "help.h"

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
void timer(enum timer ti,int id,char reset){
	static Uint32 ti_max[TIMER_NUM];
	static Uint32 ti_sum[TIMER_NUM];
	static Uint32 ti_cnt[TIMER_NUM];
	static Uint32 last=0, lastp=0;
	Uint32 now=SDL_GetTicks();
	if(ti!=tim) return;
	if(id>=0 && id<TIMER_NUM && last){
		Uint32 diff=now-last;
		if(ti_max[id]<diff) ti_max[id]=diff;
		ti_sum[id]+=diff;
		ti_cnt[id]++;
	}
	last=now;
	if(now-lastp>2000){
		if(lastp){
			int i,l;
			char tmp[256];
			snprintf(tmp,256,"timer:");
			for(l=TIMER_NUM-1;l>=0 && !ti_cnt[l];) l--;
			for(i=0;i<=l;i++) snprintf(tmp+strlen(tmp),256-strlen(tmp)," %6.1f(%4i)",
					(float)ti_sum[i]/(float)ti_cnt[i],ti_max[i]);
			debug(DBG_NONE,tmp);
		}
		if(reset){
			memset(ti_max,0,sizeof(Uint32)*TIMER_NUM);
			memset(ti_sum,0,sizeof(Uint32)*TIMER_NUM);
			memset(ti_cnt,0,sizeof(Uint32)*TIMER_NUM);
		}
		lastp=now;
	}
}

enum debug dbg = DBG_NONE;
enum debug logdbg = DBG_NONE;
#define E(X)	#X
const char *dbgstr[] = { EDEBUG };
#undef E

FILE *fdlog=NULL;

void debug_ex(enum debug lvl,const char *file,int line,const char *txt,...){
	va_list ap;
	char err = lvl==ERR_QUIT || lvl==ERR_CONT;
	FILE *fout = err ? stderr : stdout;
	char fcp[5];
	char *pos;
	int i;
	if(!err && lvl>dbg && (!fdlog || lvl>logdbg)) return;
	if((pos=strrchr(file,'/'))) file=pos+1;
	snprintf(fcp,5,file);
	if((pos=strchr(fcp,'.'))) *pos='\0';
	for(i=0;i<2;i++,fout=fdlog){
		if(!i && !err && lvl>dbg) continue;
		if(!fout) continue;
		fprintf(fout,"%-4s(%3i) ",fcp,line);
		if(err) fprintf(fout,"ERROR   : ");
		else    fprintf(fout,"dbg-%-4s: ",dbgstr[lvl]);
		va_start(ap,txt);
		vfprintf(fout,_(txt),ap);
		va_end(ap);
		fprintf(fout,"\n");
	}
	if(lvl!=ERR_QUIT) return;
	SDL_Quit();
	exit(1);
}

int mprintf(const char *format,...){
	va_list ap;
	int ret;
	va_start(ap,format);
	ret=vfprintf(stdout,format,ap);
	va_end(ap);
	if(!fdlog) return ret;
	va_start(ap,format);
	vfprintf(fdlog,format,ap);
	va_end(ap);
	return ret;
}


char *progpath=NULL;

const char *getprogpath(){ return progpath; }

char *finddatafile(const char *fn){
	int i;
	static char ret[FILELEN];
	static const char *dirs[]={NULL, NULL, ".", "data", "../data", DATADIR, NULL};
	if(progpath && !dirs[0]){
		size_t l=strlen(progpath);
		char *tmp;
		if(l){
			dirs[0]=tmp=malloc(l);
			snprintf(tmp,l,progpath);
		}
		dirs[1]=tmp=malloc(l+5);
		snprintf(tmp,l+5,"%sdata",progpath);
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

const char *gettmp(){
	const char *fn;
	fn=getenv("TEMP");
	if(!fn) fn=getenv("TMP");
	if(!fn) fn="/tmp";
	return fn;
}

struct mainthread {
	int (*fnc)(void *);
	int pri;
	char init;
	void *data;
	SDL_Thread *pt;
} mainthreads[] = {
	{ &sdlthread, 15, 0, NULL },
	{ &dplthread, 10, 0, NULL },
	{ &ldthread,   5, 0, NULL },
	{ &actthread,  2, 0, NULL },
	{ &mapldthread,1, 0, (void*)0 },
	{ &mapldthread,1, 0, (void*)1 },
	{ NULL,        0, 0, NULL },
};

int threadid(){
	struct mainthread *mt=mainthreads;
	Uint32 id=SDL_ThreadID();
	int i=0;
	for(;mt->fnc;mt++,i++) if(mt->init && mt->pt && SDL_GetThreadID(mt->pt)==id) return i;
	return 0;
}

size_t nthreads(){
	struct mainthread *mt=mainthreads;
	size_t n=0;
	for(;mt->fnc;mt++) n++;
	return n;
}

void start_threads(){
	struct mainthread *mt=mainthreads;
	mainthreads->pt=NULL;
	mainthreads->init=1;
	for(;mt->fnc;mt++) if(!mt->init){
		mt->pt=SDL_CreateThread(mt->fnc,mt->data);
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
	return i!=0;
}

void setprogpath(char *pfn){
	size_t i;
	for(i=strlen(pfn);i--;) if(pfn[i]=='/' || pfn[i]=='\\'){
		progpath=malloc(i+2);
		memcpy(progpath,pfn,i+1);
		progpath[i+1]='\0';
		break;
	}
}

void fileoutput(char doopen){
	switch(doopen){
	case 1: {
			const char *paths[2];
			int i;
			paths[0]=progpath?progpath:"";
			paths[1]=getenv("TEMP");
			for(i=0;!fdlog && i<2;i++) if(paths[i]){
				char *fn;
				size_t l=strlen(paths[i]);
				fn=malloc(l+9);
				snprintf(fn,l+9,"%s%slog.txt",paths[i],(l && paths[i][l-1]!='/' && paths[i][l-1]!='\\')?"\\":"");
				fdlog=fopen(fn,"w");
				free(fn);
			}
	} break;
	case 2: {
		const char *logfn=cfggetstr("main.log");
		if(!logfn || !logfn[0]) return;
		if(fdlog) fclose(fdlog);
		fdlog=fopen(logfn,"w");
	} break;
	case 0:
		if(fdlog){
			fclose(fdlog);
			fdlog=NULL;
		}
	break;
	}
}

#ifdef __WIN32__
#define watchcoredump(X,Y,Z)	0
#else
int watchcoredump(int *ret,int argc,char **argv){
	const char *dir=cfggetstr("main.coredump");
	pid_t pid;
	int status;
	if(!dir || !dir[0]) return 0;
	pid=fork();
	if(!pid){
		struct rlimit rl;
		getrlimit(RLIMIT_CORE,&rl);
		rl.rlim_cur=rl.rlim_max;
		setrlimit(RLIMIT_CORE,&rl);
	}
	if(pid<=0) return 0;
	waitpid(pid,&status,0);
	*ret = WIFEXITED(status) ? WIFEXITED(status) : 1;
	if(WIFSIGNALED(status) && WCOREDUMP(status)){
		char cdir[FILELEN];
		char cmd[FILELEN*2];
		time_t t=time(NULL);
		struct tm *tm=localtime(&t);
		int i;
		FILE *fd;
		snprintf(cdir,FILELEN,"%s/core_dump",dir);
		mkdirm(cdir);
		snprintf(cdir,FILELEN,"%s/core_dump/%04i%02i%02i_%02i%02i%02i",dir,
			tm->tm_year+1900,tm->tm_mon+1,tm->tm_mday,
			tm->tm_hour,tm->tm_min,tm->tm_sec);
		mkdirm(cdir);
		snprintf(cmd,FILELEN*2,"mv core %s/",cdir); system(cmd);
		snprintf(cmd,FILELEN*2,"cp %s/build/slideshowgl %s/",dir,cdir); system(cmd);
		snprintf(cmd,FILELEN*2,"svn info %s >%s/svn-info",dir,cdir); system(cmd);
		snprintf(cmd,FILELEN*2,"svn diff %s >%s/svn-diff",dir,cdir); system(cmd);
		snprintf(cmd,FILELEN*2,"%s/cmd",cdir);
		fd=fopen(cmd,"w");
		for(i=0;i<argc;i++) fprintf(fd,"%s\n",argv[i]);
		fclose(fd);

	}
	return 1;
}
#endif

int main(int argc,char **argv){
	int ret=0;
	if(argc) setprogpath(argv[0]);
#ifdef __WIN32__
	fileoutput(1);
#endif
	srand((unsigned int)time(NULL));
	cfgparseargs(argc,argv);
	fileoutput(2);
	if(watchcoredump(&ret,argc,argv)) goto end;
	dbg=cfggetint("main.dbg");
	logdbg=cfggetint("main.logdbg");
	if(dbg>logdbg) logdbg=dbg;
	tim=cfggetenum("main.timer");
	fgetfiles(argc-optind,argv+optind);
	sdlinit();
	start_threads();
	if(!end_threads())
		error(ERR_CONT,"sdl timeout waiting for threads:%s%s%s%s%s%s",
			(sdl_quit&THR_SDL)?"":" sdl",
			(sdl_quit&THR_DPL)?"":" dpl",
			(sdl_quit&THR_LD )?"":" ld",
			(sdl_quit&THR_ACT)?"":" act",
			(sdl_quit&THR_ML1)?"":" mapld1",
			(sdl_quit&THR_ML2)?"":" mapld2");
	else sdlquit();
end:
	fileoutput(0);
	return ret;
}
