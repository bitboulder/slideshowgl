#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <GL/glew.h>
#if HAVE_CURL
	#undef DATADIR
	#include <curl/curl.h>
#endif
#include "mapele.h"
#include "map_int.h"
#include "mapld.h"
#include "help.h"
#include "cfg.h"
#include "sdl.h"

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
	int tw,th;
	float rw,rh;
	char loading;
	GLuint tex;
	unsigned char *v;
};

#define N_ETEXLOAD	16
struct etexload {
	int wi,ri;
	struct meld *buf[N_ETEXLOAD];
};

#define MEHNUM	128
struct mapele {
	struct meld *ld[MEHNUM];
	const char *cachedir;
	const char *url;
	struct etexload tl;
} mapele = {
	.ld={ NULL },
};

void etexloadput(struct meld *ld){
	int nwi=(mapele.tl.wi+1)%N_ETEXLOAD;
	while(nwi==mapele.tl.ri){
		if(sdl_quit) return;
		SDL_Delay(10);
	}
	ld->loading=1;
	mapele.tl.buf[mapele.tl.wi]=ld;
	mapele.tl.wi=nwi;
}

char etexload(){
	struct meld *ld;
	if(mapele.tl.wi==mapele.tl.ri) return 0;
	ld=mapele.tl.buf[mapele.tl.ri];
	if(ld->v){
		if(!ld->tex) glGenTextures(1,&ld->tex);
		glBindTexture(GL_TEXTURE_2D,ld->tex);
		glTexImage2D(GL_TEXTURE_2D,0,GL_RED,ld->tw,ld->th,0,GL_RED,GL_UNSIGNED_BYTE,ld->v);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
		if(GLEW_EXT_texture_edge_clamp){
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE); /* TODO: correct ? */
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE); /* TODO: correct ? */
		}else{
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP);
		}
		free(ld->v); ld->v=NULL;
		ld->loading=0;
	}else{
		glDeleteTextures(1,&ld->tex);
		ld->tex=0;
	}
	mapele.tl.ri=(mapele.tl.ri+1)%N_ETEXLOAD;
	return 1;
}

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

void mapele_ld1_srtm(int gx,int gy,struct meld *ld){
	char fn[FILELEN];
	char cmd[FILELEN];
	enum filetype ft;
	FILE *fd;
	unsigned char *buf,*b;
	unsigned char *v;
	int x,y;
	int w=1201,h=1201;
	ld->tw=2048; ld->th=2048;
	ld->rw=(float)w/(float)ld->tw;
	ld->rh=(float)h/(float)ld->th;
	ld->v=calloc((size_t)(ld->tw*ld->th),sizeof(unsigned char));
	if(!mapele.cachedir || !mapele.cachedir[0]) return;
	snprintf(fn,FILELEN,"%s/srtm/%c%02i%c%03i.hgt.zip",mapele.cachedir,gy<0?'S':'N',abs(gy),gx<0?'W':'E',abs(gx));
	ft=filetype(fn);
#if HAVE_CURL
	if(ft==FT_NX) mapele_ld1_srtmget(gx,gy,fn);
	ft=filetype(fn);
#endif
	if(!(ft&FT_FILE)) return;
	snprintf(cmd,FILELEN,"unzip -p '%s'\n",fn);
	if(!(fd=popen(cmd,"r"))){
		debug(ERR_CONT,"mapele_load: file unzip failed (fn: %s)",fn);
		return;
	}
	buf=malloc((size_t)(2*w*h));
	if(fread(buf,1,(size_t)(2*w*h),fd)!=(size_t)(2*w*h)){
		debug(ERR_CONT,"mapele_load: file read failed (fn: %s)",fn);
		free(buf);
		return;
	}
	pclose(fd);
	for(v=ld->v,b=buf,y=0;y<w;y++,v+=(ld->th-h)){
		for(x=0;x<h;x++,b+=2,v++){
			int t=(b[0]<<16)+b[1];
			t-=70;
			if(t>255) *v=255;
			else if(t<0) *v=0;
			else *v=(unsigned char)t;
		}
	}
	free(buf);
}

#define MELDCH(gx,gy)	(((gx)*90+(gy))%MEHNUM)

struct meld *mapele_ldfind(int gx,int gy){
	struct meld *ld=mapele.ld[MELDCH(gx,gy)];
	while(ld && (ld->gx!=gx || ld->gy!=gy)) ld=ld->nxt;
	return ld;
}

char mapele_ld1(int gx,int gy){
	struct meld *ld=mapele_ldfind(gx,gy);
	if(ld) return 0;
	ld=malloc(sizeof(struct meld));
	ld->gx=gx;
	ld->gy=gy;
	ld->loading=0;
	ld->tex=0;
	mapele_ld1_srtm(gx,gy,ld);
	ld->nxt=mapele.ld[MELDCH(gx,gy)];
	mapele.ld[MELDCH(gx,gy)]=ld;
	etexloadput(ld);
	return 1;
}

char mapele_ld(int gx0,int gx1,int gy0,int gy1){
	int gx,gy;
	for(gx=gx0;gx<=gx1;gx++)
		for(gy=gy0;gy<=gy1;gy++) if(mapele_ld1(gx,gy)) return 1;
	return 0;
}

void mapelerenderld(struct meld *ld){
	if(!ld) return;
	if(!ld->tex) return;
	glBindTexture(GL_TEXTURE_2D,ld->tex);
	glBegin(GL_QUADS);
	glTexCoord2f(    0.,    0.); glVertex2f(0.,0.);
	glTexCoord2f(ld->rw,    0.); glVertex2f(1.,0.);
	glTexCoord2f(ld->rw,ld->rh); glVertex2f(1.,1.);
	glTexCoord2f(    0.,ld->rh); glVertex2f(0.,1.);
	glEnd();
}

void mapelerender(double gsx0,double gsx1,double gsy0,double gsy1){
	int gx0=(int)trunc(gsx0);
	int gx1=(int)trunc(gsx1);
	int gy0=(int)trunc(gsy0);
	int gy1=(int)trunc(gsy1);
	int gx,gy;
	while(etexload()) ;
	glPushMatrix();
	glScaled(1./(gsx1-gsx0),1./(gsy1-gsy0),1.);
	glTranslated(trunc(gsx0)-gsx0,gsy1-trunc(gsy1),0.);
	for(gx=gx0;gx<=gx1;gx++){
		for(gy=gy0;gy<=gy1;gy++){
			mapelerenderld(mapele_ldfind(gx,gy));
			glTranslatef(0.f,-1.f,0.f);
		}
		glTranslatef(1.f,(float)(gy1-gy0),0.f);
	}
	glPopMatrix();
}

void mapeleinit(const char *cachedir){
	mapele.cachedir=cachedir;
	mapele.url=cfggetstr("mapele.url");
}
