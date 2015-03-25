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
#include "gl_rnd.h"
#include "zlib.h"

#define MAXREFIZ	7

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

#define MEHNUM	128
struct mapele {
	struct meld *ld[MEHNUM];
	const char *cachedir;
	const char *url;
	struct metexload tl;
	short maxmin[2];
	struct meld ldbar,ldref;
	unsigned short *uid;
} mapele = {
	.ld={ NULL },
	.tl.wi=0,
	.tl.ri=0,
	.uid=NULL,
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

void mapeleuidinit(){
	const char *fn;
	gzFile fd;
	unsigned short *uid;
	if(!(fn=finddatafile("srtm.map.gz"))){ debug(ERR_CONT,"mapeleuidinit: srtm.map.gz not found"); return; }
	if(!(fd=gzopen(fn,"rb"))){ debug(ERR_CONT,"mapeleuidinit: file '%s' not readable",fn); return; }
	uid=malloc(36*180*sizeof(unsigned short));
	if(36*180*sizeof(unsigned short)!=gzread(fd,uid,36*180*sizeof(unsigned short))){ debug(ERR_CONT,"mapeleuidinit: file '%s' read failure",fn); return; }
	gzclose(fd);
	mapele.uid=uid;
}

const char *mapeleuidget(int gx,int gy){
	unsigned short v=mapele.uid[(gy+90)*36+(gx+180)/10];
	int i=v&(1<<((gx+180)%10));
	switch((v>>(i?13:10))&0x7){
	case 1: return "Africa";
	case 2: return "Australia";
	case 3: return "Eurasia";
	case 4: return "Islands";
	case 5: return "North_America";
	case 6: return "South_America";
	default: return NULL;
	}
}

#if HAVE_CURL
static size_t mapele_ld1_srtmdata(void *data,size_t size,size_t nmemb,void *arg){
	FILE **fd=(FILE**)arg;
	return fwrite(data,size,nmemb,*fd);
}

void mapele_ld1_srtmget(int gx,int gy,char *fn){
	char url[FILELEN*2];
	FILE *fd;
	CURL *curl;
	CURLcode res;
	struct curl_slist *lst=NULL;
	const char *uid=mapeleuidget(gx,gy);
	if(!mapele.url || !mapele.url[0] || !uid) return;
	snprintf(url,FILELEN*2,"%s/%s/%c%02i%c%03i%shgt.zip",
		mapele.url,uid,
		gy<0?'S':'N',abs(gy),gx<0?'W':'E',abs(gx),
		gx>=55 && gx<=60 && gy>=43 && gy<=174 ? "" : ".");
	mkdirm(fn,1);
	debug(DBG_STA,"mapele_ld1_strmget: download tex (fn: %s)",fn);
	curl=curl_easy_init();
	if(!curl){ error(ERR_CONT,"mapele_ld1_strmget: curl-init failed"); return; }
	if(!(fd=fopen(fn,"wb"))){
		error(ERR_CONT,"mapele_ld1_strmget: cache-file open failed (%s)",fn);
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
		debug(DBG_STA,"mapele_ld1_strmget: download failed (res: %i)",res);
		unlink(fn);
	}else
		debug(DBG_STA,"mapele_ld1_strmget: tex downloaded (fn: %s)",fn);
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
				if((mi[i*wi]<v && mi[i*wi]!=-32768) || v==-32768) v=mi[i*wi];
				if((mi[i*(wi+1)]<v && mi[i*(wi+1)]!=-32768) || v==-32768) v=mi[i*(wi+1)];
				*(mo++)=v;
				if(i>1) mi++;
				v=mi[0];
				if(mi[i]>v) v=mi[i];
				if(mi[i*wi]>v) v=mi[i*wi];
				if(mi[i*(wi+1)]>v) v=mi[i*(wi+1)];
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
	double rx0= ld->gx==1000 ? (gsx0+180)*12./(double)ld->w : gsx0-(double)ld->gx;
	double rx1= ld->gx==1000 ? (gsx1+180)*12./(double)ld->w : gsx1-(double)ld->gx;
	double ry1= ld->gy==1000 ? (90-gsy0)*12./(double)ld->h  : 1.+(double)ld->gy-gsy0;
	double ry0= ld->gy==1000 ? (90-gsy1)*12./(double)ld->h  : 1.+(double)ld->gy-gsy1;
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
	if(!mapele.cachedir || !mapele.cachedir[0]) return;
	snprintf(fn,FILELEN,"%s/srtm/%c%02i%c%03i.hgt.zip",mapele.cachedir,gy<0?'S':'N',abs(gy),gx<0?'W':'E',abs(gx));
	ft=filetype(fn);
#if HAVE_CURL
	if(ft==FT_NX) mapele_ld1_srtmget(gx,gy,fn);
	ft=filetype(fn);
#endif
	if(!(ft&FT_FILE)){
		debug(ERR_CONT,"mapele_load: tex file not found (fn: %s)",fn);
		return;
	}
	snprintf(cmd,FILELEN,"unzip -p '%s'\n",fn);
	debug(DBG_STA,"mapele_load: loading tex (fn: %s)",fn);
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
	ld->dtex=calloc((size_t)(ld->tw*ld->th),sizeof(unsigned short));
	for(dtex=ld->dtex,mm=ld->maxmin,y=0;y<ld->w;y++,dtex+=((size_t)(ld->th-ld->h)))
		for(x=0;x<ld->h;x++,mm++,dtex++){
			unsigned char *b=(unsigned char*)mm;
			*mm=(short)((b[0]<<8)+b[1]);
			/* TODO: fix -32768 */
			*dtex=(unsigned short)((int)*mm+32768);
		}
	debug(DBG_STA,"mapele_load: tex loaded (fn: %s)",fn);
}

#define MELDCH(gx,gy)	(abs((gx)*90+(gy))%MEHNUM)

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
	ld->maxmin=NULL;
	ld->dtex=NULL;
	mapele_ld1_srtm(gx,gy,ld);
	if(ld->maxmin) mapele_mmgen(ld);
	ld->nxt=mapele.ld[MELDCH(gx,gy)];
	mapele.ld[MELDCH(gx,gy)]=ld;
	if(ld->maxmin) metexloadput(ld);
	sdlforceredraw();
	return 1;
}

char mapeleload(int iz,int gx0,int gx1,int gy0,int gy1){
	int gx,gy;
	if(!glprg() || iz<=MAXREFIZ) return 0;
	/* TODO: restrict to -180..179, -90..89 */
	/* TODO: load from center */
	for(gx=gx0;gx<=gx1;gx++)
		for(gy=gy0;gy<=gy1;gy++) if(mapele_ld1(gx,gy)) return 1;
	return 0;
}

const char *mapelestat(double gsx,double gsy){
	struct meld *ld=mapele_ldfind((int)floor(gsx),(int)floor(gsy));
	int ix,iy;
	static char str[16];
	if(!ld || !ld->maxmin) return " ?m";
	ix=(int)round((gsx-floor(gsx))*(double)(ld->w-1));
	iy=(int)round((1.-(gsy-floor(gsy)))*(double)(ld->h-1));
	if(ix<0) ix=0; if(ix>=ld->w) ix=ld->w-1;
	if(iy<0) iy=0; if(iy>=ld->h) iy=ld->h-1;
	snprintf(str,16," %im",ld->maxmin[iy*ld->w+ix]);
	return str;
}

void mapelegendls(struct meld *ld){
	float xo,yo,rw,rh;
	double px0,px1,py0,py1;
	ld->dls=glGenLists(1);
	glNewList(ld->dls,GL_COMPILE);
	xo=.5f/(float)ld->w;
	yo=.5f/(float)ld->h;
	rw=(float)ld->w/(float)ld->tw;
	rh=(float)ld->h/(float)ld->th;
	if(ld->gx==1000){
		px0=py0=0.;
		px1=py1=1.;
	}else{
		mapg2p(ld->gx,  ld->gy,  px0,py0);
		mapg2p(ld->gx+1,ld->gy+1,px1,py1);
	}
	glBindTexture(GL_TEXTURE_2D,ld->tex);
	glBegin(GL_QUADS);
	glTexCoord2f(   xo,   yo); glVertex2d(px0,py1);
	glTexCoord2f(rw-xo,   yo); glVertex2d(px1,py1);
	glTexCoord2f(rw-xo,rh-yo); glVertex2d(px1,py0);
	glTexCoord2f(   xo,rh-yo); glVertex2d(px0,py0);
	glEnd();
	glEndList();
}

void mapelerenderld(int gx,int gy,int d,struct meld *ld){
	if(d==1 && ld && ld->tex){
		if(!ld->dls) mapelegendls(ld);
		glCallList(ld->dls);
	}else if(mapele.ldref.tex){
		double px0,px1,py0,py1;
		double tx0=(double)(gx+180  )/mapele.ldref.tw*12.;
		double tx1=(double)(gx+180+d)/mapele.ldref.tw*12.;
		double ty0=(double)(90-gy-d )/mapele.ldref.th*12.;
		double ty1=(double)(90-gy   )/mapele.ldref.th*12.;
		mapg2p(gx,  gy,  px0,py0);
		mapg2p(gx+d,gy+d,px1,py1);
		glBindTexture(GL_TEXTURE_2D,mapele.ldref.tex);
		glBegin(GL_QUADS);
		glTexCoord2d(tx0,ty0); glVertex2d(px0,py1);
		glTexCoord2d(tx1,ty0); glVertex2d(px1,py1);
		glTexCoord2d(tx1,ty1); glVertex2d(px1,py0);
		glTexCoord2d(tx0,ty1); glVertex2d(px0,py0);
		glEnd();
	}
}

void mapelerender(double px,double py,int iz,double gsx0,double gsx1,double gsy0,double gsy1){
	int gx0=(int)floor(gsx0);
	int gx1=(int)floor(gsx1);
	int gy0=(int)floor(gsy0);
	int gy1=(int)floor(gsy1);
	int gx,gy;
	struct meld *ld;
	int d = iz<=MAXREFIZ ? MAXREFIZ-iz+2 : 1;
	char mmref=0;
	while(metexload()) ;
	mapele.maxmin[0]=32767;
	mapele.maxmin[1]=-32768;
	/* TODO: restrict to -180..179, -90..89 */
	if(d==1) for(gx=gx0;gx<=gx1;gx++) for(gy=gy0;gy<=gy1;gy++)
		if((ld=mapele_ldfind(gx,gy)) && ld->maxmin) mapele_mmget(mapele.maxmin,ld,gsx0,gsx1,gsy0,gsy1);
		else mmref=1;
	if((d!=1 || mmref) && mapele.ldref.maxmin) mapele_mmget(mapele.maxmin,&mapele.ldref,gsx0,gsx1,gsy0,gsy1);
	glPushMatrix();
	glTranslated(-px,-py,0.);
	glseccol(1.f,1.f,0.f);
	glColor4d((double)mapele.maxmin[0]/65535.+0.5,(double)(mapele.maxmin[1]-mapele.maxmin[0])/65535.,0.,.8);
	for(gx=gx0;gx<=gx1;gx+=d) for(gy=gy0;gy<=gy1;gy+=d)
		mapelerenderld(gx,gy,d,d==1?mapele_ldfind(gx,gy):NULL);
	glseccol(1.f,0.f,0.f);
	glColor4f(.5f,.5f,.5f,1.f);
	glPopMatrix();
}

#define MEBARSIZE	128

void mapelerenderbar(){
	struct istat *stat=effstat();
	float hrat,sw,h,w,b;
	int si,vi;
	float rf,sf,vf;
	if(!stat->h || !glfontsel(FT_NOR)) return;
	sw=glmode(GLM_TXT);
	glTranslatef(0.f,-.5f,0.f);
	glScalef(1.f,stat->h,1.f);

	h=glfontscale(hrat=glcfg()->hrat_stat,1.f);
	hrat/=h;
	w=(float)sw/hrat*.4f;
	b=h*glcfg()->txt_border*2.f;

	si=(mapele.maxmin[1]-mapele.maxmin[0])/5;
	for(vi=1;si>=20;vi*=10) si/=10;
	if(si>=15) si=15;
	else if(si>=10) si=10;
	else if(si==9) si=8;
	else if(si==7) si=6;
	else if(si==0) si=1;
	si*=vi;
	rf=w/(float)(mapele.maxmin[1]-mapele.maxmin[0]);
	sf=rf*(float)si;
	vi=(mapele.maxmin[0]/si)*si;
	if(vi<mapele.maxmin[0]) vi+=si;
	vf=w/2.f-rf*(float)(vi-mapele.maxmin[0]);

	glColor4fv(glcfg()->col_txtbg);
	glrectarc(w+b*2.f,b+h*2.f,GP_BOTTOM|GP_HCENTER,-b-h*2.f);

	glColor4fv(glcfg()->col_txtfg);
	glPushMatrix();
	glTranslatef(-vf,(h+b)/2.f,0.f);
	for(;vi<=mapele.maxmin[1];vi+=si){
		char str[16];
		snprintf(str,16,"%i",vi);
		glfontrender(str,GP_VCENTER|GP_HCENTER);
		glTranslatef(sf,0.f,0.f);
	}
	glPopMatrix();

	glmodeslave(GLM_2D);
	glTranslatef(-w/2.f,h+b,0.f);
	glScalef(w,h-2.f*b,1.f);
	glseccol(1.f,1.f,0.f);
	glColor4d(0.,(MEBARSIZE-1)/65535.,0.,1.);
	mapelerenderld(0,0,1,&mapele.ldbar);
	glseccol(1.f,0.f,0.f);
	glColor4f(.5f,.5f,.5f,1.f);
}

void mapelebarinit(struct meld *ld){
	unsigned short i;
	ld->loading=0;
	ld->gx=ld->gy=1000;
	ld->tex=0;
	ld->dls=0;
	ld->w=ld->tw=MEBARSIZE;
	ld->h=ld->th=1;
	ld->dtex=malloc((size_t)(ld->tw*ld->th)*sizeof(unsigned short));
	for(i=0;i<ld->w;i++) ld->dtex[i]=i;
	metexloadput(ld);
}

char mapelerefread(short *m,int mn,gzFile fd){
	short *mi=m;
	short *me=m+mn;
	unsigned int v;
	while(mi<me){
		int n=3;
		v=0; if(3!=gzread(fd,&v,3)) return 1;
		mi+=v+1;
		if(mi==me) return 0;
		if(mi>me) return 1;
		while(n==3){
			int k;
			if(4!=gzread(fd,&v,4)) return 1;
			n=(int)(v>>30);
			if(mi+n+1>me) return 1;
			for(k=0;k<=n && k<3;k++){
				int vi=(v>>k*10)&1023;
				*(mi++)=(short)(vi*8-512);
			}
		}
		
	}
	return 0;
}

void mapelerefinit(struct meld *ld){
	const char *fn;
	gzFile fd;
	int x,y;
	ld->loading=0;
	ld->gx=ld->gy=1000;
	ld->tex=0;
	ld->dls=0;
	ld->w=360*12; ld->tw=8192;
	ld->h=180*12; ld->th=4096;
	ld->maxmin=NULL;
	ld->dtex=NULL;
	if(!(fn=finddatafile("mapref.gz"))){ debug(ERR_CONT,"mapelerefinit: mapref not found"); return; }
	if(!(fd=gzopen(fn,"rb"))){ debug(ERR_CONT,"mapelerefinit: file '%s' not readable",fn); return; }
	ld->maxmin=calloc((size_t)mapele_mmsize(ld),sizeof(short));
	if(mapelerefread(ld->maxmin,ld->w*ld->h,fd)){ debug(ERR_CONT,"mapelerefinit: file '%s' read failure",fn); free(ld->maxmin); ld->maxmin=NULL; return; }
	gzclose(fd);
	mapele_mmgen(ld);
	ld->dtex=calloc((size_t)(ld->tw*ld->th),sizeof(unsigned short));
	for(y=0;y<ld->h;y++) for(x=0;x<ld->w;x++)
		ld->dtex[y*ld->tw+x]=(unsigned short)((int)ld->maxmin[y*ld->w+x]+32768);
	metexloadput(ld);
}

void mapeleinit(const char *cachedir){
	mapele.cachedir=cachedir;
	mapele.url=cfggetstr("mapele.url");
	mapelebarinit(&mapele.ldbar);
	mapelerefinit(&mapele.ldref);
	mapeleuidinit();
}
