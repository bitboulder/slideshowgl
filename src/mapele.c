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
#include "gl.h"

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
	int w,h;
	char loading;
	GLuint tex;
	GLuint dls;
	short *maxmin;
		/* tw   x th   x 1 : value   (1201x1201x2)
		 * tw/2 x th/2 x 2 : min,max (600x600x2)
		 * tw/4 x th/4 x 2 : min,max (300x300x2)
		 * ...
		 * tw/512 x th/512 x 2 : min,max (2x2x2)
		 */
	unsigned short *dtex;
};

#define N_ETEXLOAD	16
struct metexload {
	int wi,ri;
	struct meld *buf[N_ETEXLOAD];
};

struct medls {
	struct medls *nxt;
	int w,h;
	GLuint dls;
};

#define MEHNUM	128
struct mapele {
	struct meld *ld[MEHNUM];
	const char *cachedir;
	const char *url;
	struct metexload tl;
	struct medls *dls;
} mapele = {
	.ld={ NULL },
	.tl.wi=0,
	.tl.ri=0,
	.dls=NULL,
};

void metexloadput(struct meld *ld){
	int nwi=(mapele.tl.wi+1)%N_ETEXLOAD;
	while(nwi==mapele.tl.ri){
		if(sdl_quit) return;
		SDL_Delay(10);
	}
	ld->loading=1;
	mapele.tl.buf[mapele.tl.wi]=ld;
	mapele.tl.wi=nwi;
}

char metexload(){
	struct meld *ld;
	if(mapele.tl.wi==mapele.tl.ri) return 0;
	ld=mapele.tl.buf[mapele.tl.ri];
	if(ld->dtex){
		if(!ld->tex) glGenTextures(1,&ld->tex);
		glBindTexture(GL_TEXTURE_2D,ld->tex);
		glTexImage2D(GL_TEXTURE_2D,0,GL_R16,ld->tw,ld->th,0,GL_RED,GL_UNSIGNED_SHORT,ld->dtex);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
		if(GLEW_EXT_texture_edge_clamp){
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE); /* TODO: correct ? */
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE); /* TODO: correct ? */
		}else{
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP);
		}
		free(ld->dtex); ld->dtex=NULL;
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

int mapele_mmsize(struct meld *ld){
	int wi=ld->w>>1;
	int hi=ld->h>>1;
	int s=ld->w*ld->h;
	for(;wi>0 || hi>0;wi>>=1,hi>>=1) s+=wi*hi*2;
	return s;
}

void mapele_mmgen(struct meld *ld){
	int wi=ld->w, wo;
	int hi=ld->h, ho;
	short *mi=ld->maxmin;
	short *mo=ld->maxmin+wi*hi;
	int x,y,i=1;
	for(;wi>0 || hi>0;wi=wo,hi=ho,i=2){
		wo=wi>>1;
		ho=hi>>1;
		for(y=0;y<ho;y++){
			for(x=0;x<wo;x++){
				short v;
				v=mi[0];
				if((mi[i]<v && mi[i]!=-32768) || v==-32768) v=mi[i];
				if((mi[i*hi]<v && mi[i*hi]!=-32768) || v==-32768) v=mi[i*hi];
				if((mi[i*(hi+1)]<v && mi[i*(hi+1)]!=-32768) || v==-32768) v=mi[i*(hi+1)];
				*(mo++)=v;
				if(i>1) mi++;
				v=mi[0];
				if(mi[i]>v) v=mi[i];
				if(mi[i*hi]>v) v=mi[i*hi];
				if(mi[i*(hi+1)]>v) v=mi[i*(hi+1)];
				*(mo++)=v;
				mi++;
				mi+=i;
			}
			if(wi%2) mi+=i;
			mi+=i*wi;
		}
		if(hi%2) mi+=i*wi;
	}
}

#define MMUPD(s) { \
	if(v[0]!=-32768 && v[0]<mm[0]) mm[0]=v[0]; \
	if(v[s]!=-32768 && v[s]>mm[1]) mm[1]=v[s]; \
}

void mapele_mmget(short *mm,struct meld *ld,double gsx0,double gsx1,double gsy0,double gsy1){
	double rx0=gsx0-(double)ld->gx,    rx1=gsx1-(double)ld->gx;
	double ry1=1.+(double)ld->gy-gsy0, ry0=1.+(double)ld->gy-gsy1;
	int ix0 = rx0<=0. ? 0     : (int)round(rx0*(double)ld->w);
	int ix1 = rx1>=1. ? ld->w : (int)round(rx1*(double)ld->w);
	int iy0 = ry0<=0. ? 0     : (int)round(ry0*(double)ld->h);
	int iy1 = ry1>=1. ? ld->h : (int)round(ry1*(double)ld->h);
	int wi=ld->w, hi=ld->h;
	int i;
	short *v,*vm=ld->maxmin;
	if(ix0==ix1 || iy0==iy1) return;
	if(ix0==0 && iy0==0 && ix1==ld->w && iy1==ld->h){
		v=ld->maxmin+mapele_mmsize(ld)-2;
		MMUPD(1);
		return;
	}
	if(ix0%2) for(v=vm+iy0*wi+ix0,    i=iy0;i<iy1;v+=wi,i++) MMUPD(0);
	if(ix1%2) for(v=vm+iy0*wi+ix1-1,  i=iy0;i<iy1;v+=wi,i++) MMUPD(0);
	if(iy0%2) for(v=vm+iy0*wi+ix0,    i=ix0;i<ix1;v++  ,i++) MMUPD(0);
	if(iy1%2) for(v=vm+(iy1-1)*wi+ix0,i=ix0;i<ix1;v++  ,i++) MMUPD(0);
	vm+=wi*hi;
	wi>>=1; hi>>=1;
	for(;wi>0 || hi>0;wi>>=1,hi>>=1){
		ix0=(ix0+1)>>1; ix1>>=1;
		iy0=(iy0+1)>>1; iy1>>=1;
		if(ix0==ix1 || iy0==iy1) return;
		if(ix0%2) for(v=vm+(iy0*wi+ix0)*2,    i=iy0;i<iy1;v+=wi*2,i++) MMUPD(1);
		if(ix1%2) for(v=vm+(iy0*wi+ix1-1)*2,  i=iy0;i<iy1;v+=wi*2,i++) MMUPD(1);
		if(iy0%2) for(v=vm+(iy0*wi+ix0)*2,    i=ix0;i<ix1;v+=2   ,i++) MMUPD(1);
		if(iy1%2) for(v=vm+((iy1-1)*wi+ix0)*2,i=ix0;i<ix1;v+=2   ,i++) MMUPD(1);
		vm+=wi*hi*2;
	}
}

void mapele_ld1_srtm(int gx,int gy,struct meld *ld){
	char fn[FILELEN];
	char cmd[FILELEN];
	enum filetype ft;
	FILE *fd;
	unsigned short *dtex;
	short *mm;
	int x,y;
	ld->w=1201; ld->h=1201;
	ld->tw=2048; ld->th=2048;
	ld->dtex=calloc((size_t)(ld->tw*ld->th),sizeof(unsigned short));
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
	ld->maxmin=malloc(sizeof(short)*(size_t)mapele_mmsize(ld));
	if(fread(ld->maxmin,1,sizeof(short)*(size_t)(ld->w*ld->h),fd)!=sizeof(short)*(size_t)(ld->w*ld->h)){
		debug(ERR_CONT,"mapele_load: file read failed (fn: %s)",fn);
		return;
	}
	pclose(fd);
	for(dtex=ld->dtex,mm=ld->maxmin,y=0;y<ld->w;y++,dtex+=((size_t)(ld->th-ld->h)))
		for(x=0;x<ld->h;x++,mm++,dtex++){
			unsigned char *b=(unsigned char*)mm;
			*mm=(short)((b[0]<<8)+b[1]);
			/* TODO: fix -32768 */
			*dtex=(unsigned short)((int)*mm+32768);
		}
	mapele_mmgen(ld);
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
	ld->dls=0;
	mapele_ld1_srtm(gx,gy,ld);
	ld->nxt=mapele.ld[MELDCH(gx,gy)];
	mapele.ld[MELDCH(gx,gy)]=ld;
	metexloadput(ld);
	sdlforceredraw();
	return 1;
}

char mapele_ld(int gx0,int gx1,int gy0,int gy1){
	int gx,gy;
	if(!glprg()) return 0;
	for(gx=gx0;gx<=gx1;gx++)
		for(gy=gy0;gy<=gy1;gy++) if(mapele_ld1(gx,gy)) return 1;
	return 0;
}

void mapelegendls(struct meld *ld){
	struct medls *dls=mapele.dls;
	while(dls && dls->w!=ld->w && dls->h!=ld->h) dls=dls->nxt;
	if(dls) ld->dls=dls->dls; else {
		float xo,yo,rw,rh;
		dls=malloc(sizeof(struct meld));
		dls->w=ld->w;
		dls->h=ld->h;
		dls->dls=glGenLists(1);
		glNewList(dls->dls,GL_COMPILE);
		xo=.5f/(float)ld->w;
		yo=.5f/(float)ld->h;
		rw=(float)ld->w/(float)ld->tw;
		rh=(float)ld->h/(float)ld->th;
		glBegin(GL_QUADS);
		glTexCoord2f(   xo,   yo); glVertex2f(0.,1.);
		glTexCoord2f(rw-xo,   yo); glVertex2f(1.,1.);
		glTexCoord2f(rw-xo,rh-yo); glVertex2f(1.,0.);
		glTexCoord2f(   xo,rh-yo); glVertex2f(0.,0.);
		glEnd();
		glEndList();
		dls->nxt=mapele.dls;
		mapele.dls=dls;
		ld->dls=dls->dls;
	}
}

void mapelerenderld(struct meld *ld){
	if(!ld) return;
	if(!ld->tex) return;
	if(!ld->dls) mapelegendls(ld);
	glBindTexture(GL_TEXTURE_2D,ld->tex);
	glCallList(ld->dls);
}

void mapelerender(double gsx0,double gsx1,double gsy0,double gsy1){
	int gx0=(int)trunc(gsx0);
	int gx1=(int)trunc(gsx1);
	int gy0=(int)trunc(gsy0);
	int gy1=(int)trunc(gsy1);
	int gx,gy;
	short mm[2]={32767,-32768};
	struct meld *ld;
	while(metexload()) ;
	for(gx=gx0;gx<=gx1;gx++)
		for(gy=gy0;gy<=gy1;gy++)
			if((ld=mapele_ldfind(gx,gy))) mapele_mmget(mm,ld,gsx0,gsx1,gsy0,gsy1);
	glPushMatrix();
	glTranslatef((float)gx0,(float)gy0,0.f);
	glSecondaryColor3f(1.f,1.f,0.f); /* TODO: only if gl.ver>1.4f */
	glColor4d((double)mm[0]/65535.+0.5,(double)(mm[1]-mm[0])/65535.,0.,.8);
	for(gx=gx0;gx<=gx1;gx++){
		for(gy=gy0;gy<=gy1;gy++){
			mapelerenderld(mapele_ldfind(gx,gy));
			glTranslatef(0.f,1.f,0.f);
		}
		glTranslatef(1.f,(float)(gy0-gy1+1),0.f);
	}
	glSecondaryColor3f(1.f,0.f,0.f);
	glColor4f(.5f,.5f,.5f,1.f);
	glPopMatrix();
}

void mapeleinit(const char *cachedir){
	mapele.cachedir=cachedir;
	mapele.url=cfggetstr("mapele.url");
}
