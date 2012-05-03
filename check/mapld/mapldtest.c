#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>

#include "mapld.h"
#include "main.h"

#define E(X)	#X
const char *dbgstr[] = { EDEBUG };
#undef E
void debug_ex(enum debug lvl,const char *file,int line,const char *txt,...){
	va_list ap;
	if(lvl==ERR_QUIT||lvl==ERR_CONT) printf("ERROR"); else printf("dbg-%-4s",dbgstr[lvl]);
	printf(" at %s:%i: ",file,line);
	va_start(ap,txt);
	vprintf(txt,ap);
	va_end(ap);
	printf("\n");
}
char mkdirm(const char *dir,char file){ return 1; }
long filesize(const char *fn){ return 1; }

#define N_TIS 6
struct mapld {
	int wi,ri;
	struct mapldti {
		const char *maptype;
		int iz,ix,iy;
	} tis[N_TIS];
	char cachedir[FILELEN];
	char init;
	void *mutex;
};

extern struct mapld mapld;

static size_t mapld_load_data(void *data,size_t size,size_t nmemb,void *arg){
	FILE **fd=(FILE**)arg;
	return fwrite(data,size,nmemb,*fd);
}

int main(){
	struct mapldti ti={ .maptype="gs", .iz=12, .ix=2204, .iy=1370 };
	char cmd[FILELEN];
	snprintf(mapld.cachedir,FILELEN,"/tmp");
	snprintf(cmd,FILELEN,"mkdir -p %s/%s/%i/%i",mapld.cachedir,ti.maptype,ti.iz,ti.ix); system(cmd);
	mapld_load(ti);
	snprintf(cmd,FILELEN,"mv %s/%s/%i/%i/%i.png .",mapld.cachedir,ti.maptype,ti.iz,ti.ix,ti.iy); system(cmd);
	snprintf(cmd,FILELEN,"rm -r %s/%s",mapld.cachedir,ti.maptype); system(cmd);
	return 0;
}
