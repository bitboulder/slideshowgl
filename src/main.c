#include "config.h"
#ifndef __WIN32__
	#define _POSIX_C_SOURCE 200809L
	#define	_DEFAULT_SOURCE
	#include <features.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <SDL.h>
#include <time.h>
#if SDL_THREAD_PTHREAD && HAVE_PTHREAD
	#include <pthread.h>
#endif
#ifndef __WIN32__
	#include <sys/types.h>
	#include <sys/wait.h>
	#include <sys/time.h>
	#include <sys/resource.h>
	#include <dirent.h>
	#include <signal.h>
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
#include "exich.h"
#include "act.h"
#include "map.h"

enum debug dbg = DBG_NONE;
enum debug logdbg = DBG_NONE;
#define E(X)	#X
const char *dbgstr[] = { EDEBUG };
#undef E

FILE *fdlog=NULL;
int errcnt=0;

char optx=0;

void debug_ex(enum debug lvl,const char *file,int line,const char *txt,...){
	va_list ap;
	char err = lvl==ERR_QUIT || lvl==ERR_CONT;
	FILE *fout = err ? stderr : stdout;
	char fcp[5];
	char *pos;
	int i;
	if(!err && lvl>dbg && (!fdlog || lvl>logdbg)) return;
	if(err) errcnt++;
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
	const char *name;
	int (*fnc)(void *);
	SDL_ThreadPriority pri;
	char init;
	void *data;
	/* not initialized */
	SDL_threadID id;
#if SDL_THREAD_PTHREAD && HAVE_PTHREAD
	pthread_t pt;
#endif
} mainthreads[] = {
	{ "sdl",  &sdlthread,  SDL_THREAD_PRIORITY_HIGH,   0, NULL },
	{ "dpl",  &dplthread,  SDL_THREAD_PRIORITY_NORMAL, 0, NULL },
	{ "ld",   &ldthread,   SDL_THREAD_PRIORITY_LOW,    0, NULL },
	{ "act",  &actthread,  SDL_THREAD_PRIORITY_LOW,    0, NULL },
	{ "map1", &mapldthread,SDL_THREAD_PRIORITY_LOW,    0, (void*)0 },
//	{ "map2", &mapldthread,SDL_THREAD_PRIORITY_LOW,    0, (void*)1 },
	{ NULL,   NULL,        SDL_THREAD_PRIORITY_LOW,    0, NULL },
};

int threadid(){
	struct mainthread *mt=mainthreads;
	SDL_threadID id=SDL_ThreadID();
	int i=0;
	for(;mt->fnc;mt++,i++) if(mt->init && mt->id==id) return i;
	return 0;
}

size_t nthreads(){
	struct mainthread *mt=mainthreads;
	size_t n=0;
	for(;mt->fnc;mt++) n++;
	return n;
}

int run_thread(void *arg){
	struct mainthread *mt=(struct mainthread *)arg;
	SDL_SetThreadPriority(mt->pri);
	mt->id=SDL_ThreadID();
#if SDL_THREAD_PTHREAD && HAVE_PTHREAD
	mt->pt=pthread_self();
#endif
	mt->init=1;
	return mt->fnc(mt->data);
}

void start_threads(){
	struct mainthread *mt=mainthreads;
	mainthreads->id=SDL_ThreadID();
	mainthreads->init=1;
	for(;mt->fnc;mt++) if(!mt->init) SDL_CreateThread(run_thread,mt->name,mt);
	run_thread(mainthreads);
}

char end_threads(){
	int i;
	for(i=500;(sdl_quit&THR_OTH)!=THR_OTH && i>0;i--) SDL_Delay(10);
	return i!=0;
}

enum timer tim;
#define TIMER_NUM	16
#define TDIFF(a,b)	( (double)((b).tv_sec-(a).tv_sec)*1e3 +  (double)((b).tv_nsec-(a).tv_nsec)/1e6 )
void timerx(enum timer ti,int id,char reset,double count){
	static double ti_max[TIMER_NUM];
	static double ti_sum[TIMER_NUM];
	static double ti_cnt[TIMER_NUM];
	static double ti_lst[TIMER_NUM];
	static struct timespec last={0,0}, lastp={0,0};
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC,&now);
	if(ti!=tim) return;
	if(ti==TI_THR){
#if SDL_THREAD_PTHREAD && HAVE_PTHREAD
		struct mainthread *mt=mainthreads;
		int i;
		for(i=0;mt->fnc;mt++,i++) if(mt->pt){
			clockid_t cid;
			struct timespec time;
			double t;
			pthread_getcpuclockid(mt->pt, &cid);
			clock_gettime(cid,&time);
			t=(double)time.tv_sec*100000.+(double)time.tv_nsec/10000.;
			if(ti_lst[i]) ti_sum[i]+=t-ti_lst[i];
			ti_lst[i]=t;
			ti_cnt[i]=1;
		}
#endif
	}else if(id>=0 && id<TIMER_NUM && last.tv_nsec){
		double diff=TDIFF(last,now);
		if(ti_max[id]<diff) ti_max[id]=diff;
		ti_sum[id]+=diff;
		ti_cnt[id]+=count;
	}
	last=now;
	if(TDIFF(lastp,now)>2000){
		if(lastp.tv_nsec){
			int i,l;
			char tmp[256];
			snprintf(tmp,256,"timer:");
			for(l=TIMER_NUM-1;l>=0 && !ti_cnt[l];) l--;
			for(i=0;i<=l;i++)
				if(ti==TI_THR) snprintf(tmp+strlen(tmp),256-strlen(tmp)," %6.1f(%-4s)",
					ti_sum[i]/TDIFF(lastp,now),mainthreads[i].name);
				else snprintf(tmp+strlen(tmp),256-strlen(tmp)," %6.1f(%4.0f)",
					ti_sum[i]/ti_cnt[i],ti_max[i]);
			debug(DBG_NONE,tmp);
		}
		if(reset){
			memset(ti_max,0,sizeof(double)*TIMER_NUM);
			memset(ti_sum,0,sizeof(double)*TIMER_NUM);
			memset(ti_cnt,0,sizeof(double)*TIMER_NUM);
		}
		lastp=now;
	}
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
	static const char *logfn=NULL;
	const char *logfn2=NULL;
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
				logfn=fn;
			}
	} break;
	case 2:
		logfn2=cfggetstr("main.log");
		if(!logfn2 || !logfn2[0]) break;
	case 0:
		if(fdlog){
			fclose(fdlog);
			fdlog=NULL;
			if(!errcnt && logfn && logfn[0] && cfggetbool("main.dellog")) unlink(logfn);
		}
		if(logfn2) fdlog=fopen(logfn=logfn2,"w");
	break;
	}
}

#if defined __WIN32__ || defined DEBUG
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
		char core[64];
		char cdir[FILELEN];
		char cmd[FILELEN*2];
		time_t t=time(NULL);
		struct tm *tm=localtime(&t);
		int i;
		FILE *fd;
		snprintf(core,64,"core.%i",pid);
		if(filetype(core)!=FT_FILE) snprintf(core,64,"core");
		snprintf(cdir,FILELEN,"%s/core_dump/%04i%02i%02i_%02i%02i%02i",dir,
			tm->tm_year+1900,tm->tm_mon+1,tm->tm_mday,
			tm->tm_hour,tm->tm_min,tm->tm_sec);
		mkdirm(cdir,0);
		snprintf(cmd,FILELEN*2,"mv %s %s/",core,cdir); system(cmd);
		snprintf(cmd,FILELEN*2,"cp %s/build/slideshowgl %s/",dir,cdir); system(cmd);
		snprintf(cmd,FILELEN*2,"bash -c 'cd %s; git log -n 1 >%s/git-log'",dir,cdir); system(cmd);
		snprintf(cmd,FILELEN*2,"bash -c 'cd %s; git diff >%s/git-diff'",dir,cdir); system(cmd);
		snprintf(cmd,FILELEN*2,"%s/cmd",cdir);
		fd=fopen(cmd,"w");
		for(i=0;i<argc;i++) fprintf(fd,"%s\n",argv[i]);
		fclose(fd);
	}
	return 1;
}
#endif

char terminate(){
#ifndef __WIN32__
	char exe_self[FILELEN];
	pid_t pid_self=getpid();
	DIR *dir;
	struct dirent *de;
	long pid_kill=0;
	if(readlink("/proc/self/exe",exe_self,FILELEN)<=0) return 0;
	if(!(dir=opendir("/proc"))) return 0;
	while((de=readdir(dir))){
		size_t l=0;
		ssize_t ll;
		long pid;
		char fexe[FILELEN];
		char exe[FILELEN];
		for(;l<NAME_MAX && de->d_name[l];l++) if(de->d_name[l]<'0' || de->d_name[l]>'9') break;
		if(l>=NAME_MAX || de->d_name[l]) continue;
		snprintf(fexe,MIN(FILELEN,l+1),de->d_name);
		pid=strtol(fexe,NULL,10);
		if(pid<=0 || pid==LONG_MAX) continue;
		if(pid==pid_self) continue;
		snprintf(fexe,FILELEN,"/proc/%li/exe",pid);
		if((ll=readlink(fexe,exe,FILELEN))<=0) continue;
		if(strncmp(exe,exe_self,MIN(FILELEN,(size_t)ll))) continue;
		if(!pid_kill || pid_kill<pid) pid_kill=pid; /* TODO: select by mem space */
	}
	closedir(dir);
	if(!pid_kill) return 0;
	kill((pid_t)pid_kill,SIGINT);
	return 1;
#else
	return 0;
#endif
}

int main(int argc,char **argv){
	int ret=0;
//	if(terminate()) return 0;
	if(argc) setprogpath(argv[0]);
#ifdef __WIN32__
	fileoutput(1);
#endif
	srand((unsigned int)time(NULL));
	cfgparseargs(argc,argv);
	fileoutput(2);
	if(watchcoredump(&ret,argc,argv)) return ret;
	dbg=cfggetint("main.dbg");
	logdbg=cfggetint("main.logdbg");
	if(dbg>logdbg) logdbg=dbg;
	tim=cfggetenum("main.timer");
	actinit();
	exichload();
	fgetfiles(argc-optind,argv+optind);
	sdlinit();
	mapinit();
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
	fileoutput(0);
	return errcnt;
}
