#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#if HAVE_CURL
	#undef DATADIR
	#include <curl/curl.h>
#endif
#include "mapele.h"
#include "map_int.h"
#include "mapld.h"
#include "help.h"
#include "cfg.h"

/* SRTM3 Format
 
 cfggetstr("map.ele.url") -> [pos]=N51E013

 unzip -p FILE | >> 1201 x 1201 x short

 51°/13° => links unten
 52°/13° => Jüterbog	52°/14° => Lübben
 				N51E013
 51°/13° => Mittweida	51°/14° => Wünschendorf
 				N50E013
 50°/13° => Touzim		50°/14° => Kaldno

 N51E013 0			=> 52°/13°
 N51E013 1200		=> 52°/14°
 N51E013 1441200	=> 51°/13°
 N51E013 1442400	=> 51°/14° = N50E013 1200 = N50E014 0 = N51E014 1441200

 px = round(rest(x)*1200) + (1200-round(rest(y)*1200))*1201

 { unzip -p N51E013.hgt.zip; unzip -p N50E013.hgt.zip; } | od --endian=big -vt d2 -w2402 -Anone | plot.pl -typ image -cbrange 0:500 -C "set yrange [-0.5:3602.5] reverse" -xrange -0.5:1200.5 -title 50-51/13
*/

struct meld {
	struct meld *nxt;
	int gx,gy;
	int w,h;
	float *v;
};

#define MEHNUM	128
struct mapele {
	struct meld *ld[MEHNUM];
	const char *cachedir;
	const char *url;
} mapele = {
	.ld={ NULL },
};

#if HAVE_CURL
static size_t mapele_ld1_srtmdata(void *data,size_t size,size_t nmemb,void *arg){
	FILE **fd=(FILE**)arg;
	return fwrite(data,size,nmemb,*fd);
}

void mapele_ld1_srtmget(int gx,int gy,char *fn){
	char url[FILELEN*2],*pos;
	size_t l=0;
	FILE *fd;
	CURL *curl;
	CURLcode res;
	struct curl_slist *lst=NULL;
	if(!mapele.url || !mapele.url[0]) return;
	pos=strstr(mapele.url,"[pos]");
	if(!pos) return;
	snprintf(url,FILELEN*2,"%s",mapele.url);
	l=(size_t)(pos-mapele.url);
	l+=(size_t)snprintf(url+l,FILELEN*2-l,"%c%02i%c%03i",gy<0?'S':'N',abs(gy),gx<0?'W':'E',abs(gx));
	snprintf(url+l,FILELEN*2-l,"%s",pos+5);
	mkdirm(fn,1);
	curl=curl_easy_init();
	if(!curl){ error(ERR_CONT,"mapele_load: curl-init failed"); return; }
	if(!(fd=fopen(fn,"wb"))){
		error(ERR_CONT,"mapele_load: cache-file open failed (%s)",fn);
		curl_easy_cleanup(curl);
		return;
	}
	curl_easy_setopt(curl,CURLOPT_URL,url);
	curl_easy_setopt(curl,CURLOPT_USERAGENT,"Opera/9.80 (X11; Linux x86_64; U; de) Presto/2.9.168 Version/11.50");
	lst=curl_slist_append(lst,"Accept-Language: de");
	lst=curl_slist_append(lst,"Accept: */*");
	curl_easy_setopt(curl,CURLOPT_HTTPHEADER,lst);
	curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,mapele_ld1_srtmdata);
	curl_easy_setopt(curl,CURLOPT_WRITEDATA,&fd);
	res=curl_easy_perform(curl);
	curl_slist_free_all(lst);
	curl_easy_cleanup(curl);
	fclose(fd);
	if(res!=CURLE_OK || !filesize(fn) || !mapld_filecheck(fn)){
		debug(DBG_STA,"mapele_load: file loading failed (res: %i)",res);
		unlink(fn);
	}
}
#endif

#define SRTMW	1201
#define SRTMH	1201

void mapele_ld1_srtm(int gx,int gy,float *v){
	char fn[FILELEN];
	char cmd[FILELEN];
	enum filetype ft;
	FILE *fd;
	char buf[SRTMW*SRTMH*2],*b=buf;
	int i;
	if(!mapele.cachedir || !mapele.cachedir[0]) return;
	snprintf(fn,FILELEN,"%s/srtm/%c%02i%c%03i.hgt.zip",mapele.cachedir,gy<0?'S':'N',abs(gy),gx<0?'W':'E',abs(gx));
	ft=filetype(fn);
	if(ft==FT_NX) mapele_ld1_srtmget(gx,gy,fn);
	ft=filetype(fn);
	if(!(ft&FT_FILE)) return;
	snprintf(cmd,FILELEN,"unzip -p '%s'\n",fn);
	if(!(fd=popen(cmd,"r"))){
		debug(ERR_CONT,"mapele_load: file unzip failed (fn: %s)",fn);
		return;
	}
	if(fread(buf,1,2*SRTMW*SRTMH,fd)!=2*SRTMW*SRTMH){
		debug(ERR_CONT,"mapele_load: file read failed (fn: %s)",fn);
		return;
	}
	pclose(fd);
	for(i=SRTMW*SRTMH;i>=0;i--,b+=2,v++){
		short t;
		((char *)&t)[1]=b[0];
		((char *)&t)[2]=b[1];
		*v=t;
	}
}

char mapele_ld1(int gx,int gy){
	int ch=(gx*90+gy)%MEHNUM;
	struct meld *ld=mapele.ld[ch];
	while(ld && (ld->gx!=gx || ld->gy!=gy)) ld=ld->nxt;
	if(ld) return 0;
	ld=malloc(sizeof(struct meld));
	ld->gx=gx;
	ld->gy=gy;
	ld->w=SRTMW-1;
	ld->h=SRTMH-1;
	ld->v=calloc((size_t)((ld->w+1)*(ld->h+1)),sizeof(float));
	mapele_ld1_srtm(gx,gy,ld->v);
	ld->nxt=mapele.ld[ch];
	mapele.ld[ch]=ld;
	return 1;
}

char mapele_ld(int gx0,int gx1,int gy0,int gy1){
	int gx,gy;
	for(gx=gx0;gx<=gx1;gx++)
		for(gy=gy0;gy<=gy1;gy++) if(mapele_ld1(gx,gy)) return 1;
	return 0;
}

void mapeleinit(const char *cachedir){
	mapele.cachedir=cachedir;
	mapele.url=cfggetstr("mapele.url");
}
