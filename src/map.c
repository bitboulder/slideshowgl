#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <GL/glew.h>
#include <SDL.h>
#include <SDL_image.h>
#if HAVE_OPENDIR
	#include <sys/types.h>
	#include <dirent.h>
	#ifndef NAME_MAX
		#define NAME_MAX 255
	#endif
#endif
#include <time.h> // TODO: remove
#include "map.h"
#include "img.h"
#include "help.h"
#include "main.h"
#include "file.h"
#include "cfg.h"
#include "sdl.h"
#include "gl_int.h"
#include "file.h"
#include "act.h"
#include "eff.h"     //ISTAT_TXTSIZE
#include "dpl_int.h" //dplwritemode

/* Coordinate abreviations *
 *
 * g - double - gps
 * m - double - map global value
 * i - int    - map tile index
 * i - int    - number of tiles on screen
 * o - float  - offset within map tile
 * s - float  - screen position (-0.5..0.5)
 */

#define N_ZOOM	18

#define MAPTYPE E(om)
#define E(X)	MT_##X
enum maptype { MAPTYPE, MT_NUM };
#undef E
#define E(X)	#X
const char *maptype_str[]={ MAPTYPE };
#undef E

struct mappos {
	double gx,gy;
	int iz;
};

struct tile {
	int ix,iy,iz;
	char loading;
	GLuint tex;
};

#define N_TEXLOAD	16
struct texload {
	int wi,ri;
	struct {
		struct tile *ti;
		SDL_Surface *sf;
	} buf[N_TEXLOAD];
};

struct mapimg {
	struct mapimg *nxt;
	char dir[FILELEN];
	char *name;
	double gx,gy;
};

struct mapclti {
	struct mapclti *nxtimg;
	struct mapimg  *img;
	struct mapclti *nxtclt;
	int nimg;
	double mx,my;
};

struct mapclt {
	struct mapclti *cltbuf;
	struct mapclti *clts;
};

struct mapimgs {
	struct mapimg *img;
	struct mapclt clt[N_ZOOM];
} mapimgs;

struct map {
	char init;
	char *basedirs;
	enum maptype maptype;
	struct mappos pos;
	struct mappos possave;
	struct tile ****tile;
	char cachedir[FILELEN];
	struct texload tl;
	int *scr_w, *scr_h;
	struct tile imgdir[2];
	int info;
} map = {
	.init = 2,
	.basedirs = NULL,
	.maptype = MT_om,
	.pos.gx = 13.732001,
	.pos.gy = 51.088938,
	.pos.iz = 12,
	.tile = NULL,
	.tl.wi = 0,
	.tl.ri = 0,
	.scr_w = NULL,
	.scr_h = NULL,
	.info  = -1,
};

char mapon(){ return !strncmp(ilfn(ilget(0)),"[MAP]",5); }
void mapsdlsize(int *w,int *h){ map.scr_w=w; map.scr_h=h; }
void mapswtype(){
	map.maptype=(map.maptype+1)%MT_NUM;
	sdlforceredraw();
}
void mapinfo(int i){
	if(map.info==i) return;
	map.info=i;
	sdlforceredraw();
}
const char *mapgetbasedirs(){ return map.basedirs; }

#define mapp2i(pos,i)	mapg2i(pos.gx,pos.gy,pos.iz,&i##x,&i##y)

void mapg2m(double gx,double gy,int iz,double *mx,double *my){
	double size=(double)(1<<iz);
	gy=gy/180.*M_PI;
	gy=asinh(tan(gy))/M_PI/2.;
	gy=0.5-gy;
	gx=gx/360.+.5;
	*mx=gx*size;
	*my=gy*size;
}

void mapm2g(double mx,double my,int iz,double *gx,double *gy){
	double size=(double)(1<<iz);
	mx/=size;
	my/=size;
	my=0.5-my;
	my=atan(sinh(my*2.*M_PI));
	if(gy) *gy=my*180./M_PI;
	if(gx) *gx=(mx-.5)*360.;
}

void mapg2i(double gx,double gy,int iz,int *ix,int *iy){
	double mx,my;
	mapg2m(gx,gy,iz,&mx,&my);
	*ix=(int)mx;
	*iy=(int)my;
}

void mapg2o(double gx,double gy,int iz,float *ox,float *oy){
	double mx,my;
	mapg2m(gx,gy,iz,&mx,&my);
	mx-=floor(mx);
	my-=floor(my);
	*ox=(float)(.5-mx);
	*oy=(float)(.5-my);
}

void maps2g(float sx,float sy,int iz,double *gx,double *gy){
	double mx,my;
	if(!map.scr_w || !map.scr_h) return;
	mapg2m(map.pos.gx,map.pos.gy,iz,&mx,&my);
	mx+=(double)sx/256.**map.scr_w;
	my+=(double)sy/256.**map.scr_h;
	mapm2g(mx,my,iz,gx,gy);
}

void mapm2s(double mx,double my,int iz,float *sx,float *sy){
	double mxg,myg;
	if(!map.scr_w || !map.scr_h) return;
	mapg2m(map.pos.gx,map.pos.gy,iz,&mxg,&myg);
	if(sx) *sx=(float)((mx-mxg)*256./ (double)*map.scr_w);
	if(sy) *sy=(float)((my-myg)*256./ (double)*map.scr_h);
}

struct tile *maptilefind(int ix,int iy,int iz,char create){
	if(iz<0 || iz>=N_ZOOM) return NULL;
	if(ix<0 || ix>=1<<iz ) return NULL;
	if(iy<0 || iy>=1<<iz ) return NULL;
	if(!map.tile){
		if(!create) return NULL;
		map.tile=calloc(MT_NUM,sizeof(struct tile **));
	}
	if(!map.tile[map.maptype]){
		if(!create) return NULL;
		map.tile[map.maptype]=calloc(N_ZOOM,sizeof(struct tile **));
	}
	if(!map.tile[map.maptype][iz]){
		if(!create) return NULL;
		map.tile[map.maptype][iz]=calloc((size_t)1<<iz,sizeof(struct tile *));
	}
	if(!map.tile[map.maptype][iz][ix]){
		if(!create) return NULL;
		map.tile[map.maptype][iz][ix]=calloc((size_t)1<<iz,sizeof(struct tile));
	}
	if(create){
		map.tile[map.maptype][iz][ix][iy].ix=ix;
		map.tile[map.maptype][iz][ix][iy].iy=iy;
		map.tile[map.maptype][iz][ix][iy].iz=iz;
	}
	return map.tile[map.maptype][iz][ix]+iy;
}

const char *mapimgname(const char *dir){
	size_t c=0,i,len=strlen(dir);
	for(i=len;i>0 && c<2;i--) if(dir[i-1]=='/' || dir[i-1]=='\\') c++;
	if(i) i++;
	return dir+i;
}

void mapimgadd(const char *dir,double gx,double gy,char clt){
	struct mapimg *img;
	const char *name=mapimgname(dir);
	for(img=mapimgs.img;img;img=img->nxt)
		if(!strncmp(img->name,name,FILELEN)) break;
	if(!img){
		img=malloc(sizeof(struct mapimg));
		snprintf(img->dir,FILELEN,dir);
		img->name=img->dir+(name-dir);
		img->nxt=mapimgs.img;
		mapimgs.img=img;
	}else if(img->gx==gx && img->gy==gy) return;
	img->gx=gx;
	img->gy=gy;
	if(clt){
		mapimgclt(map.pos.iz);
		actadd(ACT_MAPCLT,NULL);
	}
}

size_t mapimgcltnimg(){
	struct mapimg *img;
	size_t nimg=0;
	for(img=mapimgs.img;img;img=img->nxt) nimg++;
	return nimg;
}

struct mapclt mapimgcltinit(int iz,size_t nimg){
	struct mapclt clt;
	struct mapimg *img;
	size_t i;
	clt.cltbuf=calloc((size_t)nimg,sizeof(struct mapclti));
	clt.clts=clt.cltbuf;
	for(img=mapimgs.img,i=0;i<nimg;img=img->nxt,i++){
		clt.cltbuf[i].nxtimg=NULL;
		clt.cltbuf[i].img=img;
		clt.cltbuf[i].nxtclt=i+1<nimg ? clt.cltbuf+i+1 : NULL;
		clt.cltbuf[i].nimg=1;
		mapg2m(img->gx,img->gy,iz,&clt.cltbuf[i].mx,&clt.cltbuf[i].my);
	}
	return clt;
}

struct mapcltd {
	double d;
	int done;
	struct mapclti *clti[2];
};
#define MAPCLT_MINTHR	(2./256.)
#define MAPCLT_MAXTHR	(25./256.)

void mapimgcltdheapify(struct mapcltd *cltd,size_t ncltd,size_t pn,struct mapcltd cd){
	register size_t p2,p=pn;
	register double d=cd.d;
	if(p<1 || p>ncltd) return;
	while(p>1){
		p2=p>>1;
		if(cltd[p2].d<=d) break;
		cltd[p]=cltd[p2];
		// updatepos [p]
		p=p2;
	}
	while(1){
		register double d1,d2;
		register size_t p3;
		p2=p<<1;
		if(p2>ncltd) break;
		p3=p2+1;
		d2=p3>ncltd ? 1000. : cltd[p3].d;
		d1=cltd[p2].d;
		if(d2<d){ if(d1>=d2) p2=p3; }else if(d1>=d) break;
		cltd[p]=cltd[p2];
		// updatepos [p]
		p=p2;
	}
	cltd[p]=cd;
	// updatepos [p]
}

double mapimgcltddist(struct mapclti **ci){
	double xd=ci[0]->mx/(double)ci[0]->nimg-ci[1]->mx/(double)ci[1]->nimg;
	double yd=ci[0]->my/(double)ci[0]->nimg-ci[1]->my/(double)ci[1]->nimg;
	return sqrt(xd*xd+yd*yd);
}

char mapimgcltdheapifyd(struct mapcltd *cltd,size_t *ncltd,size_t pn,struct mapclti **ci,int todo){
	struct mapcltd cd = {
		.clti={ci[0],ci[1]},
		.done=todo,
		.d=mapimgcltddist(ci),
	};
	if(cd.d>MAPCLT_MAXTHR*3.){
		ncltd[0]--;
		mapimgcltdheapify(cltd,ncltd[0],pn,cltd[ncltd[0]+1]);
	}else
		mapimgcltdheapify(cltd,ncltd[0],pn,cd);
	return 1;
}

char mapcltncmp(struct mapclti *a,struct mapclti *b){
	return strncmp(a->img->name,b->img->name,FILELEN)<0;
}

int mapimgcltdjoin(struct mapclti **cltijoin,struct mapclti *clti){
	struct mapclti *dst,*src0,*src1;
	int id;
	if(!cltijoin[0]->nimg || !cltijoin[1]->nimg) return -1;
	id=mapcltncmp(cltijoin[0],cltijoin[1]);
	dst=cltijoin[id];
	src0=cltijoin[!id];
	dst->nimg+=src0->nimg;
	dst->mx+=src0->mx;
	dst->my+=src0->my;
	src0->nimg=0;
	for(;clti;clti=clti->nxtclt) if(clti->nxtclt==src0)
		clti->nxtclt=src0->nxtclt;
	src1=dst->nxtimg;
	while(src0 || src1){
		if(!src1 || (src0 && mapcltncmp(src1,src0))){
			dst->nxtimg=src0;
			dst=src0;
			src0=src0->nxtimg;
		}else{
			dst->nxtimg=src1;
			dst=src1;
			src1=src1->nxtimg;
		}
	}
	return id;
}

size_t mapimgcltdgen(struct mapcltd *cltd,size_t *ncltd,struct mapclti *clti,double minthr){
	size_t i=0;
	struct mapclti *ci[2];
	double d;
	for(ci[0]=clti;ci[0];ci[0]=ci[0]->nxtclt) for(ci[1]=ci[0]->nxtclt;ci[1];ci[1]=ci[1]->nxtclt,i++)
		if((d=mapimgcltddist(ci))<minthr) mapimgcltdjoin(ci,clti);
	for(ci[0]=clti;ci[0];ci[0]=ci[0]->nxtclt) for(ci[1]=ci[0]->nxtclt;ci[1];ci[1]=ci[1]->nxtclt,i++){
		ncltd[0]++;
		mapimgcltdheapifyd(cltd,ncltd,ncltd[0],ci,0);
	}
	return i;
}

void mapimgcltdupdate(struct mapcltd *cltd,size_t *ncltd,struct mapclti *cmp,int todo){
	size_t p;
	for(p=ncltd[0];p>=1;) if(p<=ncltd[0] && cltd[p].done!=todo && (cltd[p].clti[0]==cmp || cltd[p].clti[1]==cmp)){
		if(todo<0){
			ncltd[0]--;
			mapimgcltdheapify(cltd,ncltd[0],p,cltd[ncltd[0]+1]);
		}else
			mapimgcltdheapifyd(cltd,ncltd,p,cltd[p].clti,todo);
	}else p--;
}

/* thread: act */
void mapimgclt(int izsel){
	int iz;
	size_t nimg=mapimgcltnimg();
	struct mapcltd *cltd=malloc((nimg*(nimg-1)/2+1)*sizeof(struct mapcltd));
	for(iz=izsel<0?0:izsel;izsel<0 ? iz<N_ZOOM : iz==izsel;iz++){
		size_t ncltd=0;
		struct mapclt clt=mapimgcltinit(iz,nimg);
		double thr=iz==N_ZOOM-1 ? 0. : MAPCLT_MAXTHR;
		int todo=0;
		mapimgcltdgen(cltd,&ncltd,clt.clts,iz==N_ZOOM-1 ? 0. : MAPCLT_MINTHR);
		while(ncltd && cltd[1].d<=thr){
			struct mapcltd cd=cltd[1];
			int dstid=mapimgcltdjoin(cd.clti,clt.clts);
			if(dstid<0) break;
			mapimgcltdupdate(cltd,&ncltd,cd.clti[!dstid],-1);
			mapimgcltdupdate(cltd,&ncltd,cd.clti[dstid],++todo);
		}
		if(ncltd && cltd[1].d<=thr){
			error(ERR_CONT,"mapimgclt: join of pre-join clusters");
			if(clt.cltbuf) free(clt.cltbuf);
			continue;
		}
		if(mapimgs.clt[iz].cltbuf) free(mapimgs.clt[iz].cltbuf);
		mapimgs.clt[iz]=clt;
		if(iz==map.pos.iz) sdlforceredraw();
	}
	free(cltd);
}

struct mapclti *mapfindclt(int i){
	struct mapclti *clti;
	if(map.init || !mapon()) return NULL;
	for(clti=mapimgs.clt[map.pos.iz].clts;clti && i;clti=clti->nxtclt) i--;
	if(!clti || clti->nimg<1) return NULL;
	return clti;
}

/* thread: dpl */
char mapgetclt(int i,struct imglist **il,const char **fn,const char **dir){
	struct mapclti *clti=mapfindclt(i);
	if(!clti) return 0;
	if(clti->nimg==1){
		*fn=clti->img->dir;
		*dir=clti->img->name;
	}else{
		*il=ilnew("[MAP-SEL]",_("[Map-Selection]"));
		for(;clti;clti=clti->nxtimg)
			faddfile(*il,clti->img->dir,NULL,0);
	}
	return 1;
}

/* thread: sdl */
char mapgetcltpos(int i,float *sx,float *sy){
	struct mapclti *clti=mapfindclt(i);
	if(!clti) return 0;
	mapm2s(clti->mx/(double)clti->nimg,clti->my/(double)clti->nimg,map.pos.iz,sx,sy);
	return 1;
}

char mapgetgps(const char *dir,double *gx,double *gy,char clt){
	double in[2];
	char fn[FILELEN];
	FILE *fd;
	int scan;
	if(!dir) return 0;
	snprintf(fn,FILELEN,"%s/gps.txt",dir);
	if(!(fd=fopen(fn,"r"))) return 0;
	scan=fscanf(fd,"%lg,%lg",in,in+1);
	fclose(fd);
	if(scan!=2) return 0;
	if(gx) *gx=in[1];
	if(gy) *gy=in[0];
	mapimgadd(dir,in[1],in[0],clt);
	return 1;
}

struct imglist *mapsetpos(int imgi){
	struct img *img;
	const char *fn;
	char set=0;
	double gx=0.,gy=0.;
	struct imglist *il;
	if(map.init) return NULL;
	if((fn=ilfn(ilget(0))) && isdir(fn)==1){
		set=mapgetgps(fn,&gx,&gy,1);
	}
	if(!set && (img=imgget(0,imgi)) && (fn=imgfilefn(img->file))){
		char *pos;
		if(isdir(fn)==1){
			set=mapgetgps(fn,&gx,&gy,1);
		}else if((pos=strrchr(fn,'/'))){
			size_t len=(size_t)(pos-fn);
			char buf[FILELEN];
			memcpy(buf,fn,len);
			buf[len]='\0';
			set=mapgetgps(buf,&gx,&gy,1);
		}
	}
	if(set){
		map.pos.gx=gx;
		map.pos.gy=gy;
		map.pos.iz=15;
	}
	if(ilfind("[MAP]",&il,1)) return il;
	return ilnew("[MAP]",_("[Map]"));
}

void texloadput(struct tile *ti,SDL_Surface *sf){
	int nwi=(map.tl.wi+1)%N_TEXLOAD;
	while(nwi==map.tl.ri){
		if(sdl_quit) return;
		SDL_Delay(10);
	}
	ti->loading=1;
	map.tl.buf[map.tl.wi].ti=ti;
	map.tl.buf[map.tl.wi].sf=sf;
	map.tl.wi=nwi;
}

char texload(){
	SDL_Surface *sf;
	struct tile *ti;
	if(map.tl.wi==map.tl.ri) return 0;
	ti=map.tl.buf[map.tl.ri].ti;
	if((sf=map.tl.buf[map.tl.ri].sf)){
		if(!ti->tex) glGenTextures(1,&ti->tex);
		glBindTexture(GL_TEXTURE_2D,ti->tex);
		glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,sf->w,sf->h,0,GL_RGB,GL_UNSIGNED_BYTE,sf->pixels);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
		if(GLEW_EXT_texture_edge_clamp){
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
		}else{
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP);
		}
		SDL_FreeSurface(sf);
		ti->loading=0;
	}else{
		glDeleteTextures(1,&ti->tex);
		ti->tex=0;
	}
	map.tl.ri=(map.tl.ri+1)%N_TEXLOAD;
	return 1;
}

char maploadtile(struct tile *ti,char web){
	char fn[FILELEN];
	const char *maptype=maptype_str[map.maptype];
	SDL_Surface *sf,*sfc;
	SDL_PixelFormat fmt;
	snprintf(fn,FILELEN,"%s/%s/%i/%i/%i.png",map.cachedir,maptype,ti->iz,ti->ix,ti->iy);
	if(!isfile(fn)){
		char cmd[FILELEN*2];
		if(!web) return 0;
		snprintf(cmd,FILELEN*2,"mkdir -p %s/%s/%i/%i/",map.cachedir,maptype,ti->iz,ti->ix);
		system(cmd);
		if(!strcmp(maptype,"om")) snprintf(cmd,FILELEN*2,"wget -qO %s http://tile.openstreetmap.org/mapnik/%i/%i/%i.png",fn,ti->iz,ti->ix,ti->iy);
		system(cmd);
		if(!filesize(fn)){ unlink(fn); return 0; }
		if(!isfile(fn)) return 0;
	}
	if(!(sf=IMG_Load(fn))) return 0;
	if(sf->w!=256 || sf->h!=256) return 0;
	memset(&fmt,0,sizeof(SDL_PixelFormat));
	fmt.BitsPerPixel=24;
	fmt.BytesPerPixel=3;
	fmt.Rmask=0x000000ff;
	fmt.Gmask=0x0000ff00;
	fmt.Bmask=0x00ff0000;
	sfc=SDL_ConvertSurface(sf,&fmt,sf->flags);
	SDL_FreeSurface(sf);
	if(!sfc) return 0;
	texloadput(ti,sfc);
	sdlforceredraw();
	return 1;
}

char mapldchecktile(int ix,int iy,int iz,char web){
	struct tile *ti=maptilefind(ix,iy,iz,1);
	if(!ti || ti->loading || ti->tex) return 0;
	debug(DBG_STA,"map loading map %i/%i/%i",iz,ix,iy);
	if(!maploadtile(ti,web)) return 0;
	return 1;
}

char mapscrtis(int *iw,int *ih){
	if(!map.scr_w || !map.scr_h) return 0;
	*iw=(*map.scr_w/512)*2+3;
	*ih=(*map.scr_h/512)*2+3;
	return 1;
}

char mapldcheck(){
	int ix,iy,ixc,iyc;
	int iw,ih,ir,r,i;
	char web;
	if(map.init || !mapon()) return 0;
	if(!mapscrtis(&iw,&ih)) return 0;
	iw=(iw-1)/2;
	ih=(ih-1)/2;
	ir=MAX(iw,ih);
	mapp2i(map.pos,i);
	for(web=0;web<2;web++){
		for(r=0;r<=ir;r++){
			for(i=0;i<(r?r*8:1);i++){
				if(!r) ixc=iyc=0;
				else if(i<r*4-2){
					ixc=(i%2?-1:1)*r;
					iyc=((i/2)%2?-1:1)*((i+2)/4);
				}else{
					ixc=((i/2)%2?-1:1)*((i-r*4+4)/4);
					iyc=(i%2?-1:1)*r;
				}
				if(mapldchecktile(ix+ixc,iy+iyc,map.pos.iz,web)) return 1;
			}
		}
	}
	return 0;
}

void mapfindgps(char *dir){
#if HAVE_OPENDIR
	size_t ldir=strlen(dir);
	DIR *dd=opendir(dir);
	struct dirent *de;
	if(!dd) return;
	while((de=readdir(dd))){
		if(de->d_name[0]=='.') continue;
		if(!strncmp(de->d_name,"gps.txt",7)){
			dir[ldir]='\0';
			mapgetgps(dir,NULL,NULL,0);
		}else{
			snprintf(dir+ldir,MAX(NAME_MAX+1,FILELEN-ldir),"/%s",de->d_name);
			if(isdir(dir)==1) mapfindgps(dir);
		}
	}
	closedir(dd);
#endif
}

void mapaddbasedir(const char *dir,const char *name){
	size_t ndir;
	char *bdir;
	if(!dir){ map.init=1; return; }
	if(!dir[0]) return;
	for(ndir=0,bdir=map.basedirs;bdir && bdir[0];bdir+=FILELEN*2) ndir++;
	map.basedirs=realloc(map.basedirs,(ndir+1)*FILELEN*2+1);
	snprintf(map.basedirs+ndir*FILELEN*2,FILELEN,dir);
	snprintf(map.basedirs+ndir*FILELEN*2+FILELEN,FILELEN,name);
	map.basedirs[(ndir+1)*FILELEN*2]='\0';
}

/* thread: act */
void mapinit(){
	const char *cd;
	char *bdir;
	while(map.init>1) SDL_Delay(500);
	mapaddbasedir(cfggetstr("map.base"),"");
	cd=cfggetstr("map.cachedir");
	memset(&mapimgs,0,sizeof(struct mapimgs));
	if(cd && cd[0]) snprintf(map.cachedir,FILELEN,cd);
	else snprintf(map.cachedir,FILELEN,"%s/slideshowgl-cache",gettmp());
	for(bdir=map.basedirs;bdir && bdir[0];bdir+=FILELEN*2){
		char dir[FILELEN];
		snprintf(dir,FILELEN,bdir);
		mapfindgps(dir);
	}
	texloadput(map.imgdir+0,IMG_Load(finddatafile("mapdir.png")));
	texloadput(map.imgdir+1,IMG_Load(finddatafile("mapdirs.png")));
	map.init=0;
	actadd(ACT_MAPCLT,NULL);
}

void maprendertile(int ix,int iy,int iz){
	struct tile *ti;
	float tx0=0.f,tx1=1.f,ty0=0.f,ty1=1.f;
	if(ix<0 || iy<0 || ix>=(1<<iz) || iy>=(1<<iz)) return;
	while(!(ti=maptilefind(ix,iy,iz,0)) || !ti->tex){
		if(!iz--) return;
		tx0/=2.f; tx1/=2.f;
		ty0/=2.f; ty1/=2.f;
		if(ix%2){ tx0+=.5f; tx1+=.5f; }
		if(iy%2){ ty0+=.5f; ty1+=.5f; }
		ix/=2;
		iy/=2;
	}
	glBindTexture(GL_TEXTURE_2D,ti->tex);
	glBegin(GL_QUADS);
	glTexCoord2f(tx0,ty0); glVertex2f(-0.5,-0.5);
	glTexCoord2f(tx1,ty0); glVertex2f( 0.5,-0.5);
	glTexCoord2f(tx1,ty1); glVertex2f( 0.5, 0.5);
	glTexCoord2f(tx0,ty1); glVertex2f(-0.5, 0.5);
	glEnd();
}

void maprendermap(){
	int ix,iy,iz;
	int iw,ih;
	float ox,oy;
	int ixc,iyc;
	if(!mapscrtis(&iw,&ih)) return;
	mapg2i(map.pos.gx,map.pos.gy,iz=map.pos.iz,&ix,&iy);
	mapg2o(map.pos.gx,map.pos.gy,iz,&ox,&oy);
	glPushMatrix();
	glScalef(256.f/(float)*map.scr_w,256.f/(float)*map.scr_h,1.f);
	glTranslatef(ox,oy,0.f);
	glTranslatef((float)(-(iw-1)/2),(float)(-(ih-1)/2),0.f);
	ix-=(iw-1)/2;
	iy-=(ih-1)/2;
	for(ixc=0;ixc<iw;ixc++){
		for(iyc=0;iyc<ih;iyc++){
			maprendertile(ix+ixc,iy+iyc,iz);
			glTranslatef(0.f,1.f,0.f);
		}
		glTranslatef(1.f,(float)-ih,0.f);
	}
	glPopMatrix();
}

void maprenderclt(){
	struct mapclti *clti;
	double mx,my;
	GLuint name=IMGI_MAP+1;
	if(!map.scr_w || !map.scr_h) return;
	mapg2m(map.pos.gx,map.pos.gy,map.pos.iz,&mx,&my);
	for(clti=mapimgs.clt[map.pos.iz].clts;clti;clti=clti->nxtclt){
		glPushMatrix();
		glScalef(256.f/(float)*map.scr_w,256.f/(float)*map.scr_h,1.f);
		glTranslated(clti->mx/(double)clti->nimg-mx,clti->my/(double)clti->nimg-my,0.);
		glScalef(15.f/256.f,10.f/256.f,1.f);
		glLoadName(name++);
		glBindTexture(GL_TEXTURE_2D,map.imgdir[clti->nxtimg?1:0].tex);
		glBegin(GL_QUADS);
		glTexCoord2f( 0.0, 0.0); glVertex2f(-0.5,-0.5);
		glTexCoord2f( 1.0, 0.0); glVertex2f( 0.5,-0.5);
		glTexCoord2f( 1.0, 1.0); glVertex2f( 0.5, 0.5);
		glTexCoord2f( 0.0, 1.0); glVertex2f(-0.5, 0.5);
		glEnd();
		glPopMatrix();
	}
	glLoadName(0);
}

/* TODO: move to gl.c */
enum glft { FT_NOR, FT_BIG, NFT };
char glfontsel(enum glft i);
float glfontscale(float hrat,float wrat);
float glfontwidth(const char *txt);
enum glpos {
	GP_LEFT    = 0x01,
	GP_RIGHT   = 0x02,
	GP_HCENTER = 0x04,
	GP_TOP     = 0x10,
	GP_BOTTOM  = 0x20,
	GP_VCENTER = 0x40,
};
#define glrect(w,h,p)	glrectarc(w,h,p,0.f);
void glrectarc(float w,float h,enum glpos pos,float barc);
void glfontrender(const char *txt,enum glpos pos);

void maprenderinfo(){
	int i;
	struct mapclti *clti,*ci;
	double mx,my,sx,sy;
	float hrat,sw,w,h,b;
	int nl,nc;
	if(map.info<0) return;
	if(!map.scr_w || !map.scr_h) return;
	for(i=0,clti=mapimgs.clt[map.pos.iz].clts;clti && i!=map.info;clti=clti->nxtclt) i++;
	if(!clti) return;
	if(!glfontsel(FT_NOR)) return;
	mapg2m(map.pos.gx,map.pos.gy,map.pos.iz,&mx,&my);
	sx=(clti->mx/(double)clti->nimg-mx)*256.f/(double)*map.scr_w;
	sy=(clti->my/(double)clti->nimg-my)*256.f/(double)*map.scr_h;
	glPushMatrix();
	sw=glmode(GLM_TXT);
	h=glfontscale(hrat=/*gl.cfg.hrat_cat*/0.03f,1.f);
	hrat/=h;
	for(w=0.f,ci=clti;ci;ci=ci->nxtimg) if((b=glfontwidth(ci->img->name))>w) w=b;
	//b=h*gl.cfg.txt_border*2.f;
	b=h*.1f*2.f;
	nl=clti->nimg; nc=1;
	if((b+h*(float)nl)*hrat>1.f){
		nl=(int)((1.f/hrat-b)/h);
		while(nl*nc<clti->nimg) nc++; // TODO
	}
	if((w+b)*(float)nc*hrat/sw+sx>.5f) sx=.5f-(w+b)*(float)nc*hrat/sw;
	if((b+h*(float)nl)*hrat+sy>.5f) sy=.5f-(b+h*(float)nl)*hrat;
	if(sx<-.5f) sx=-.5f;
	if(sy<-.5f) sy=-.5f;
	glTranslated(sx*sw/hrat,-sy/hrat,0.f);
	//glColor4fv(gl.cfg.col_txtbg);
	glColor4f(.8f,.8f,.8f,.7f);
	if(nc>1) glrect((w+b)*(float)(nc-1),b+h*(float)clti->nimg,GP_TOP|GP_LEFT);
	glTranslatef((w+b)*(float)(nc-1),0.f,0.f);
	glrect(w+b,b+h*(float)(clti->nimg-(nc-1)*nl),GP_TOP|GP_LEFT);
	glTranslatef(-(w+b)*(float)(nc-1),0.f,0.f);
	glTranslatef(b/2.f,-(b+h)/2.f,0.f);
	//glColor4fv(gl.cfg.col_txtfg);
	glColor4f(0.f,0.f,0.f,1.f);
	for(ci=clti,i=1;ci;ci=ci->nxtimg,i++){
		glfontrender(ci->img->name,GP_VCENTER|GP_LEFT);
		glTranslatef(0.f,-h,0.f);
		if(i==nl){
			i=0;
			glTranslatef(w+b,h*(float)nl,0.f);
		}
	}
	glPopMatrix();
}

char maprender(char sel){
	if(map.init || !mapon()) return 0;
	if(!sel) while(texload()) ;
	glmode(GLM_2D);
	if(glprg()) glColor4f(.5f,.5f,.5f,1.f);
	else        glColor4f(1.f,1.f,1.f,1.f);
	if(!sel) maprendermap();
	maprenderclt();
	if(!sel) maprenderinfo();
	return 1;
}

char mapscrsize(double *gw,double *gh){
	double size=(double)(1<<map.pos.iz);
	if(!map.scr_w || !map.scr_h) return 0;
	*gw=360./size;
	*gh=180./size; /* TODO */
	*gw*=(double)*map.scr_w/256.;
	*gh*=(double)*map.scr_h/256.;
	return 1;
}

char mapmove(enum dplev ev,float sx,float sy){
	int dir=DE_DIR(ev);
	double gw,gh;
	if(map.init || !mapon()) return 0;
	if(!mapscrsize(&gw,&gh)) return 1;
	if(ev&(DE_RIGHT|DE_LEFT)){
		map.pos.gx+=(double)dir*gw/3.f;
	}
	if(ev&(DE_UP|DE_DOWN)){
		map.pos.gy+=(double)dir*gh/3.f;
	}
	if(ev&(DE_ZOOMIN|DE_ZOOMOUT)){
		int iz=map.pos.iz+dir;
		double gx0,gx1,gy0,gy1;
		if(iz<0 || iz>=N_ZOOM) return 0;
		maps2g(sx,sy,map.pos.iz,&gx0,&gy0);
		maps2g(sx,sy,iz,&gx1,&gy1);
		map.pos.iz=iz;
		map.pos.gx-=gx1-gx0;
		map.pos.gy-=gy1-gy0;
	}
	sdlforceredraw();
	return 1;
}

/* thread: dpl */
char mapmovepos(float sx,float sy){
	double gx0,gx1,gy0,gy1;
	if(map.init || !mapon()) return 0;
	maps2g(0.,0.,map.pos.iz,&gx0,&gy0);
	maps2g(sx,sy,map.pos.iz,&gx1,&gy1);
	map.pos.gx+=gx1-gx0;
	map.pos.gy+=gy1-gy0;
	return 1;
}

/* thread: dpl */
char mapmarkpos(float sx,float sy,const char *dir){
	double gx,gy;
	FILE *fd;
	char buf[FILELEN];
	if(map.init || !mapon()) return 0;
	if(isdir(dir)!=1) return 0;
	maps2g(sx,sy,map.pos.iz,&gx,&gy);
	mapimgadd(dir,gx,gy,1);
	snprintf(buf,FILELEN,"%s/gps.txt",dir);
	if(!(fd=fopen(buf,"w"))) return 1;
	fprintf(fd,"%.6f,%.6f\n",gy,gx);
	fclose(fd);
	return 1;
}

char mapsearch(struct dplinput *in){
	struct mapimg *img,*imgf=NULL;
	size_t pf=1000,ilen=strlen(in->in);
	int id;
	if(map.init || !mapon()) return 0;
	in->pre[0]='\0';
	in->post[0]='\0';
	in->id=-1;
	for(id=0,img=mapimgs.img;img;img=img->nxt,id++){
		size_t n,p,nlen=strlen(img->name);
		for(n=p=0;n<=nlen-ilen;n++){
			if(img->name[n]=='/' || img->name[n]=='\\') p++;
			if(strncasecmp(img->name+n,in->in,ilen)) continue;
			if(imgf && p>pf) continue;
			if(imgf && p==pf && strncmp(img->name,imgf->name,FILELEN)<=0) continue;
			imgf=img;
			pf=p;
			in->id=id;
			snprintf(in->pre,n+1,img->name);
			snprintf(in->post,FILELEN,img->name+n+ilen);
			map.pos.gx=img->gx;
			map.pos.gy=img->gy;
		}
	}
	return 1;
}

void mapsavepos(){
	if(map.init || !mapon()) return;
	map.possave=map.pos;
}

char maprestorepos(){
	if(map.init || !mapon()) return 0;
	map.pos=map.possave;
	return 1;
}

char mapstatupdate(char *dsttxt){
	double gsx0,gsx1,gsy0,gsy1;
	char fmt[128];
	if(map.init || !mapon()) return 0;
	maps2g( .5f,.0f,map.pos.iz,&gsx0,NULL);
	maps2g(-.5f,.0f,map.pos.iz,&gsx1,NULL);
	maps2g(.0f, .5f,map.pos.iz,NULL,&gsy0);
	maps2g(.0f,-.5f,map.pos.iz,NULL,&gsy1);
	gsx0=fabs(gsx1-gsx0);
	gsy0=fabs(gsy1-gsy0);
	gsx0*=(6378.137*2.*M_PI)/360.*cos(map.pos.gy/180.*M_PI);
	gsy0*=(6356.752314*2.*M_PI)/360.;
	snprintf(fmt,128,"%%.3f%%c %%.3f%%c %%.%ifx%%.%ifkm%%s",gsx0>20.?0:gsx0>5.?1:2,gsy0>20.?0:gsy0>5.?1:2);
	snprintf(dsttxt,ISTAT_TXTSIZE,fmt,
			 fabs(map.pos.gy),map.pos.gy<0?'S':'N',
			 fabs(map.pos.gx),map.pos.gx<0?'W':'E',
			 gsx0,gsy0,
			 dplwritemode()?_(" (write-mode)"):"");
	return 1;
}

