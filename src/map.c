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
#include "map_int.h"
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
#include "mapld.h"
#include "mapele.h"

/* Coordinate abreviations *
 *
 * g - double - gps                                (-90 <= gy <= 90 | -180 <= gx <= 180)
 * p - double - map global value for zoom=0        (0 <= p < 1)
 * m - double - map global value for specific zoom (0 <= p < 2^zoom)
 * i - int    - map tile index                     (0 <= i <= 2^zoom-1)
 * i - int    - number of tiles on screen
 * o - float  - offset within map tile             (0 <= o < 1)
 * s - float  - screen position                    (-0.5 <= s <= 0.5)
 */

#define N_ZOOM	22

struct maptype *maptypes=NULL;
unsigned int nmaptypes=0;

struct tile {
	int ix,iy,iz;
	long ft;
	unsigned long ftchk;
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

enum mapimgdirs {MID_DIR,MID_DIRS,MID_STAR,MID_STARDIRS,MID_STARS};

struct mapimg {
	struct mapimg *nxt;
	char dir[FILELEN];
	int  dirid;
	char *name;
	double gx,gy;
	char star;
};

struct mapclti {
	struct mapclti *nxtimg;
	struct mapimg  *img;
	struct mapclti *nxtclt;
	int nimg;
	enum mapimgdirs id;
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
	enum mapinit {MI_ALL=0,MI_TEX=1,MI_BDIR=2,MI_NONE=3} init;
	char *basedirs;
	unsigned int maptype;
	struct mappos pos;
	struct mappos possave;
	struct tile ****tile;
	struct texload tl;
	int *scr_w, *scr_h;
	struct tile imgdir[5];
	int info;
	char ele;
	enum mapdisplaymode {MDM_ALL,MDM_NORMAL,MDM_STAR,MDM_NONE,N_MDM} displaymode;
	enum mapeditmode {MEM_ADD,MEM_REPLACE,N_MEM} editmode;
	unsigned long ftchk;
	const char *starpat;
} map = {
	.init = MI_NONE,
	.basedirs = NULL,
	.maptype = 0,
	.pos.gx = 13.733098,
	.pos.gy = 51.082175,
	.pos.iz = 12,
	.tile = NULL,
	.tl.wi = 0,
	.tl.ri = 0,
	.scr_w = NULL,
	.scr_h = NULL,
	.info  = -1,
	.ele = 0,
	.displaymode  = MDM_ALL,
	.editmode  = MEM_ADD,
	.ftchk = 3000,
};

char mapon(){
	struct imglist *il=ilget(CIL(0));
	return il ? !strncmp(ilfn(il),"[MAP]",5) : 0;
}
void mapsdlsize(int *w,int *h){ map.scr_w=w; map.scr_h=h; }
void mapswtype(){
	map.maptype=(map.maptype+1)%nmaptypes;
	sdlforceredraw();
}
void mapinfo(int i){
	if(map.info==i) return;
	map.info=i;
	sdlforceredraw();
}
const char *mapgetbasedirs(){ return map.basedirs; }
char mapdisplaymode(){
	if(!mapon()) return 0;
	map.displaymode=(map.displaymode+1)%N_MDM;
	actaddx(ACT_MAPCLT,NULL,NULL,0);
	return 1;
}
char mapelevation(){
	if(!mapon()) return 0;
	map.ele=!map.ele;
	return 1;
}
void mapeditmode(){
	if(map.init>MI_TEX || !mapon()) return;
	map.editmode=(map.editmode+1)%N_MEM;
}

char mapld_cplurl(char *url,const char *in,int x,int y,int z);
void mapaddurl(char *txt){
	char *maxz,*url;
	if(map.init<MI_NONE){ error(ERR_CONT,"map addurl with map init\n"); return; }
	if(!(maxz=strchr(txt,',')) || !(url=strchr(maxz+1,','))){ error(ERR_CONT,"map addurl parse error \"%s\"",txt); return; }
	*maxz='\0'; maxz++;
	*url='\0'; url++;
	maptypes=realloc(maptypes,sizeof(struct maptype)*(nmaptypes+1));
	if(
		snprintf(maptypes[nmaptypes].id,8,"%s",txt)<1 ||
		!(maptypes[nmaptypes].maxz=atoi(maxz)) ||
		snprintf(maptypes[nmaptypes].url,FILELEN,"%s",url)<4
	){ error(ERR_CONT,"map addurl parse error \"%s\"",txt); return; }
	debug(DBG_STA,"map addurl %s,%i,%s\n",maptypes[nmaptypes].id,maptypes[nmaptypes].maxz,maptypes[nmaptypes].url);
	nmaptypes++;
}

struct ecur *mapecur(int *iz,float *iscale){
	struct img *img;
	struct ecur *ecur;
	if((img=ilimg(CIL(0),0)) && !strncmp(imgfilefn(img->file),"[MAP]",6)){
		ecur=imgposcur(img->pos);
		if(iz){
			*iz = map.pos.iz>ecur->s ? (int)ceil(ecur->s) : (int)ecur->s;
			if(iscale) *iscale = powf(2.f,(float)*iz/ecur->s);
		}
	}else{
		ecur=NULL;
		if(iz) *iz=map.pos.iz;
	}
	return ecur;
}

#define mapg2k(gx,gy,k) { \
	(k)[0]=cos(gy/360.*M_PI*2.)*sin(gx/360.*M_PI*2.); \
	(k)[1]=sin(gy/360.*M_PI*2.); \
	(k)[2]=cos(gy/360.*M_PI*2.)*cos(gx/360.*M_PI*2.); \
}

#define mapg2p(gx,gy,px,py) { \
	(py)=(gy)/180.*M_PI; \
	(py)=asinh(tan(py))/M_PI/2.; \
	(py)=0.5-(py); \
	(px)=(gx)/360.+.5; \
}

#define mapp2g(px,py,gx,gy) { \
	(gy)=0.5-(py); \
	(gy)=atan(sinh((gy)*2.*M_PI)); \
	(gy)=(gy)*180./M_PI; \
	(gx)=((px)-.5)*360.; \
}

//	double size=(double)(1<<(iz)); 
#define mapp2m(px,py,iz,mx,my) { \
	double size=(double)(1<<(iz)); \
	(mx)=(px)*size; \
	(my)=(py)*size; \
}

#define mapm2p(mx,my,iz,px,py) { \
	double size=(double)(1<<(iz)); \
	(px)=(mx)/size; \
	(py)=(my)/size; \
}

#define mapm2i(mx,my,ix,iy) { \
	(ix)=(int)(mx); \
	(iy)=(int)(my); \
}

#define mapm2o(mx,my,ox,oy) { \
	(ox)=(float)(.5-(mx)+floor(mx)); \
	(oy)=(float)(.5-(my)+floor(my)); \
}

#define mapm2s(mx,my,iz,sx,sy) { \
	if(map.scr_w && map.scr_h){ \
		double mxg,myg; \
		mapg2m(map.pos.gx,map.pos.gy,iz,mxg,myg); \
		(sx)=(float)(((mx)-mxg)*256./ (double)*map.scr_w); \
		(sy)=(float)(((my)-myg)*256./ (double)*map.scr_h); \
	}else{ sx=sy=0.f; } \
}

#define maps2m(sx,sy,iz,mx,my) { \
	if(map.scr_w && map.scr_h){ \
		mapg2m(map.pos.gx,map.pos.gy,iz,mx,my); \
		(mx)+=(double)(sx)/256.**map.scr_w; \
		(my)+=(double)(sy)/256.**map.scr_h; \
	}else{ mx=my=0.; } \
}

#define mapg2m(gx,gy,iz,mx,my) { \
	mapg2p(gx,gy,mx,my); \
	mapp2m(mx,my,iz,mx,my); \
}

#define mapm2g(mx,my,iz,gx,gy) { \
	mapm2p(mx,my,iz,gx,gy); \
	mapp2g(gx,gy,gx,gy); \
}

#define mapm2k(mx,my,iz,k) { \
	double gx,gy; \
	mapm2g(mx,my,iz,gx,gy); \
	mapg2k(gx,gy,k); \
}

#define mapg2i(gx,gy,iz,ix,iy) { \
	double mx,my; \
	mapg2m(gx,gy,iz,mx,my); \
	mapm2i(mx,my,ix,iy); \
}

#define mapg2o(gx,gy,iz,ox,oy) { \
	double mx,my; \
	mapg2m(gx,gy,iz,mx,my); \
	mapm2o(mx,my,ox,oy); \
}

#define maps2g(sx,sy,iz,gx,gy) { \
	maps2m(sx,sy,iz,gx,gy); \
	mapm2g(gx,gy,iz,gx,gy); \
}

struct tile *maptilefind(int ix,int iy,int iz,char create){
	if(iz<0 || iz>=N_ZOOM) return NULL;
	if(ix<0 || ix>=1<<iz ) return NULL;
	if(iy<0 || iy>=1<<iz ) return NULL;
	if(!map.tile){
		if(!create) return NULL;
		map.tile=calloc(nmaptypes,sizeof(struct tile **));
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
	char *bdir;
	for(i=len;i>0 && c<3;i--) if(dir[i-1]=='/' || dir[i-1]=='\\') c++;
	if(i) i++;
	for(bdir=map.basedirs;bdir && bdir[0];bdir+=FILELEN*2) if(bdir[FILELEN*2-1]){
		size_t l=strlen(bdir);
		if(i<l && !strncmp(bdir,dir,l)){
			i=l;
			if(dir[i]=='/' || dir[i]=='\\') i++;
		}
	}
	return dir+i;
}

#define N_DIRID	128
struct mapimg *mapimgfind(const char *name,int *dirid,double gx,double gy){
	struct mapimg *img=NULL,*i;
	double dimg=0.;
	char freedirid[N_DIRID];
	for(i=mapimgs.img;i;i=i->nxt){
		if(strncmp(i->name,name,FILELEN)) continue;
		if(*dirid<0){
			double d=sqrt((gx-i->gx)*(gx-i->gx)+(gy-i->gy)*(gy-i->gy));
			if(i->dirid>=0 && i->dirid<N_DIRID) freedirid[i->dirid]=1;
			if(img && d>=dimg) continue;
			img=i; dimg=d;
		}else if(i->dirid==*dirid){ img=i; break; }
	}
	if(*dirid<0){
		int di=0;
		while(di<N_DIRID && freedirid[di]) di++;
		if(di<N_DIRID) *dirid=di;
	}
	return img;
}

void mapimgadd(const char *dir,int dirid,double gx,double gy,char clt){
	enum mapeditmode mode = dirid<0 ? map.editmode : MEM_REPLACE;
	const char *name=mapimgname(dir);
	struct mapimg *img=mapimgfind(name,&dirid,gx,gy);
	if(!img || mode==MEM_ADD){
		if(dirid<0){ error(ERR_CONT,"mapimgadd: no free dirid found for \"%s\"",dir); return; }
		img=malloc(sizeof(struct mapimg));
		snprintf(img->dir,FILELEN,dir);
		img->name=img->dir+(name-dir);
		img->dirid=dirid;
		img->nxt=mapimgs.img;
		img->star=map.starpat[0] && strstr(img->dir,map.starpat)!=NULL;
		mapimgs.img=img;
	}else if(img->gx==gx && img->gy==gy) return;
	img->gx=gx;
	img->gy=gy;
	if(clt){
		mapimgclt(map.pos.iz);
		actadd(ACT_MAPCLT,NULL,NULL);
	}
}

size_t mapimgcltnimg(){
	struct mapimg *img;
	size_t nimg=0;
	for(img=mapimgs.img;img;img=img->nxt) switch(map.displaymode){
	case MDM_ALL:                   nimg++; break;
	case MDM_NORMAL: if(!img->star) nimg++; break;
	case MDM_STAR:   if( img->star) nimg++; break;
	default:                                break;
	}
	return nimg;
}

struct mapclt mapimgcltinit(int iz,size_t nimg){
	struct mapclt clt;
	struct mapimg *img;
	size_t i;
	clt.cltbuf=calloc((size_t)nimg,sizeof(struct mapclti));
	clt.clts=clt.cltbuf;
	for(img=mapimgs.img,i=0;i<nimg;img=img->nxt){
		if(map.displaymode==MDM_NONE ||
			(map.displaymode==MDM_NORMAL &&  img->star) ||
			(map.displaymode==MDM_STAR   && !img->star))
			continue;
		clt.cltbuf[i].nxtimg=NULL;
		clt.cltbuf[i].img=img;
		clt.cltbuf[i].nxtclt=i+1<nimg ? clt.cltbuf+i+1 : NULL;
		clt.cltbuf[i].nimg=1;
		clt.cltbuf[i].id=img->star ? MID_STAR : MID_DIR;
		mapg2m(img->gx,img->gy,iz,clt.cltbuf[i].mx,clt.cltbuf[i].my);
		i++;
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

int mapimgcltdjoin(struct mapclti **cltijoin,struct mapclti **clti){
	struct mapclti *dst,*src0,*src1,*ci;
	int id;
	if(!cltijoin[0]->nimg || !cltijoin[1]->nimg) return -1;
	id=mapcltncmp(cltijoin[0],cltijoin[1]);
	dst=cltijoin[id];
	src0=cltijoin[!id];
	dst->nimg+=src0->nimg;
	dst->mx+=src0->mx;
	dst->my+=src0->my;
	src0->nimg=0;
	if(src0->id>=MID_STAR) dst->id = (dst->id>=MID_STAR || src0->id==MID_STARS) ? MID_STARS : MID_STARDIRS;
	else{
		if(dst->id==MID_DIR)  dst->id=MID_DIRS;
		if(dst->id==MID_STAR) dst->id=MID_STARDIRS;
	}
	if(clti[0]==src0) clti[0]=src0->nxtclt;
	else for(ci=clti[0];ci;ci=ci->nxtclt) if(ci->nxtclt==src0)
		ci->nxtclt=src0->nxtclt;
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

size_t mapimgcltdgen(struct mapcltd *cltd,size_t *ncltd,struct mapclti **clti,double minthr){
	size_t i=0;
	struct mapclti *ci[2];
	double d;
	for(ci[0]=clti[0];ci[0];ci[0]=ci[0]->nxtclt) for(ci[1]=ci[0]->nxtclt;ci[1];i++)
		if((d=mapimgcltddist(ci))<minthr && mapimgcltdjoin(ci,clti)==1){
			ci[0]=ci[0]->nxtclt;
			ci[1]=ci[0]->nxtclt;
		}else ci[1]=ci[1]->nxtclt;
	for(ci[0]=clti[0];ci[0];ci[0]=ci[0]->nxtclt) for(ci[1]=ci[0]->nxtclt;ci[1];ci[1]=ci[1]->nxtclt,i++){
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

void mapimgcltnorm(struct mapclti *ci){
	for(;ci;ci=ci->nxtclt){
		if(ci->nimg==0){
			error(ERR_CONT,"mapimgclt: cluster with zero elements -> correcting");
			ci->nimg=1;
		}
		ci->mx/=(double)ci->nimg;
		ci->my/=(double)ci->nimg;
	}
}

/* thread: act */
void mapimgclt(int izsel){
	int iz;
	size_t nimg=mapimgcltnimg();
	struct mapcltd *cltd=malloc((nimg*(nimg-1)/2+1)*sizeof(struct mapcltd));
	for(iz=izsel<0?0:izsel;izsel<0 ? iz<N_ZOOM : iz==izsel;iz++) if(!nimg) mapimgs.clt[iz].clts=NULL;
	else{
		size_t ncltd=0;
		struct mapclt clt=mapimgcltinit(iz,nimg);
		double thr=iz==N_ZOOM-1 ? 0. : MAPCLT_MAXTHR;
		int todo=0;
		mapimgcltdgen(cltd,&ncltd,&clt.clts,iz==N_ZOOM-1 ? 1e-6 : MAPCLT_MINTHR);
		while(ncltd && cltd[1].d<=thr){
			struct mapcltd cd=cltd[1];
			int dstid=mapimgcltdjoin(cd.clti,&clt.clts);
			if(dstid<0) break;
			mapimgcltdupdate(cltd,&ncltd,cd.clti[!dstid],-1);
			mapimgcltdupdate(cltd,&ncltd,cd.clti[dstid],++todo);
		}
		if(ncltd && cltd[1].d<=thr){
			error(ERR_CONT,"mapimgclt: join of pre-join clusters");
			if(clt.cltbuf) free(clt.cltbuf);
			continue;
		}
		mapimgcltnorm(clt.clts);
		if(mapimgs.clt[iz].cltbuf) free(mapimgs.clt[iz].cltbuf);
		mapimgs.clt[iz]=clt;
		if(iz==map.pos.iz) sdlforceredraw();
	}
	free(cltd);
	sdlforceredraw();
}

struct mapclti *mapfindclt(int i){
	struct mapclti *clti;
	if(map.init>MI_TEX || !mapon()) return NULL;
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
		for(;clti;clti=clti->nxtimg){
			char imgtxt[FILELEN],*p;
			snprintf(imgtxt,FILELEN,"%s",clti->img->name);
			while((p=strchr(imgtxt,'/')) || (p=strchr(imgtxt,'\\'))) *p='-';
			if(imgtxt[0]>='0' && imgtxt[0]<='9' &&
			   imgtxt[1]>='0' && imgtxt[1]<='9' &&
			   imgtxt[2]>='0' && imgtxt[2]<='9' &&
			   imgtxt[3]>='0' && imgtxt[3]<='9' &&
			   imgtxt[4]=='-' &&
			   imgtxt[5]>='0' && imgtxt[5]<='9' &&
			   imgtxt[6]>='0' && imgtxt[6]<='9' &&
			   imgtxt[7]=='-' &&
			   imgtxt[8]>='0' && imgtxt[8]<='9' &&
			   imgtxt[9]>='0' && imgtxt[9]<='9' &&
			   strlen(imgtxt)<FILELEN-1){
				memmove(imgtxt+11,imgtxt+10,strlen(imgtxt)-9);
				imgtxt[10]=':';
			}
			faddfile(*il,clti->img->dir,imgtxt,NULL,0);
		}
	}
	return 1;
}

/* thread: sdl */
char mapgetcltpos(int i,float *sx,float *sy){
	struct mapclti *clti=mapfindclt(i);
	if(!clti) return 0;
	mapm2s(clti->mx,clti->my,map.pos.iz,*sx,*sy);
	return 1;
}

char mapgetgps(const char *dir,double *gx,double *gy,char clt){
	double in[2];
	char fn[FILELEN];
	FILE *fd;
	int dirid;
	if(!dir) return 0;
	snprintf(fn,FILELEN,"%s/gps.txt",dir);
	if(!(fd=fopen(fn,"r"))) return 0;
	if(gx) *gx=0.;
	if(gy) *gy=0.;
	for(dirid=0;fscanf(fd,"%lg,%lg\n",in,in+1)==2;dirid++){
		if(gx) *gx=in[1];
		if(gy) *gy=in[0];
		mapimgadd(dir,dirid,in[1],in[0],clt);
	}
	fclose(fd);
	return dirid!=0;
}

struct imglist *mapsetpos(struct img *img){
	const char *fn;
	char set=0;
	double gx=0.,gy=0.;
	struct imglist *il;
	if(!nmaptypes){ error(ERR_CONT,"no maptypes defined"); return NULL; }
	if(map.init>MI_TEX){ error(ERR_CONT,"map init waiting"); return NULL; }
	if((fn=ilfn(CIL(0))) && (filetype(fn)&FT_DIR)){
		set=mapgetgps(fn,&gx,&gy,1);
	}
	if(!set && img && (fn=imgfilefn(img->file))){
		char *pos;
		if(filetype(fn)&FT_DIR){
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
	if(!ilfind("[MAP]",&il)){
		il=ilnew("[MAP]",_("[Map]"));
		faddfile(il,"[MAP]",NULL,NULL,0);
	}
	return il;
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

char maploadtile(struct tile *ti){
	char fn[FILELEN];
	SDL_Surface *sf,*sfc;
	SDL_PixelFormat fmt;
	unsigned long time=SDL_GetTicks();
	if(!ti) return 0;
	if(ti->loading==2 && ti->ftchk+map.ftchk>=time) return 0;
	ti->ftchk=time;
	ti->loading=mapld_check(map.maptype,ti->iz,ti->ix,ti->iy,!ti->loading,fn);
	if(ti->loading<2 || ti->ft>=filetime(fn)) return 0;
	ti->ft=filetime(fn);
	debug(DBG_STA,"map loading map %i/%i/%i",ti->iz,ti->ix,ti->iy);
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

char mapldchecktile(int ix,int iy,int iz){
	struct tile *ti=maptilefind(ix,iy,iz,1);
	if(!maploadtile(ti)) return 0;
	return 1;
}

char mapscrtis(int *iw,int *ih){
	if(!map.scr_w || !map.scr_h) return 0;
	*iw=(*map.scr_w/512)*2+3;
	*ih=(*map.scr_h/512)*2+3;
	return 1;
}

char mapldcheck(){
	int ix,iy,ixc,iyc,iz,izmin;
	int iw,ih,ir,r,i;
	struct ecur *ecur=mapecur(&iz,NULL);
	if(map.init>MI_TEX || !mapon() || !ecur) return 0;
	if(!mapscrtis(&iw,&ih)) return 0;
	if(map.ele){
		double gsx0,gsx1,gsy0,gsy1,t;
		maps2g(-.5f,.0f,map.pos.iz,gsx0,t);
		maps2g( .5f,.0f,map.pos.iz,gsx1,t);
		maps2g(.0f, .5f,map.pos.iz,t,gsy0);
		maps2g(.0f,-.5f,map.pos.iz,t,gsy1);
		if(mapele_ld((int)trunc(gsx0),(int)trunc(gsx1),(int)trunc(gsy0),(int)trunc(gsy1))) return 1;
	}
	if(mapldchecktile(0,0,0)) return 1;
	for(ix=0;ix<7;ix++) for(iy=0;iy<7;iy++) if(mapldchecktile(ix,iy,3)) return 1;
	if(iz>maptypes[map.maptype].maxz) iz=maptypes[map.maptype].maxz;
	iw=(iw-1)/2;
	ih=(ih-1)/2;
	ir=MAX(iw,ih);
	izmin=MAX(iz-6,0);
	for(;iz>=izmin;iz=(iz-1)&~1,ir--){
		mapg2i(ecur->x,ecur->y,iz,ix,iy);
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
				if(mapldchecktile(ix+ixc,iy+iyc,iz)) return 1;
			}
		}
	}
	return 0;
}

struct maploaddir {
	struct maploaddir *nxt;
	char dir[FILELEN];
};

void mapfindgps(struct maploaddir **dirs){
#if HAVE_OPENDIR
	struct maploaddir *lddir=dirs[0];
	char *dir=lddir->dir;
	size_t ldir=strlen(dir);
	DIR *dd=opendir(dir);
	struct dirent *de;
	*dirs=dirs[0]->nxt;
	if(!dd){ free(lddir); return; }
	while((de=readdir(dd))){
		if(de->d_name[0]=='.') continue;
		if(!strncmp(de->d_name,"gps.txt",7)){
			dir[ldir]='\0';
			mapgetgps(dir,NULL,NULL,0);
		}else{
			snprintf(dir+ldir,MAX(NAME_MAX+1,FILELEN-ldir),"/%s",de->d_name);
			if(filetype(dir)&FT_DIR){
				struct maploaddir *ndir=malloc(sizeof(struct maploaddir));
				snprintf(ndir->dir,FILELEN,dir);
				ndir->nxt=*dirs;
				*dirs=ndir;
			}
		}
	}
	closedir(dd);
	free(lddir);
	actadd(ACT_MAPCLT,NULL,NULL);
#endif
}

char maploadclt(){
	static struct maploaddir *dirs=NULL;
	if(map.init!=MI_TEX) return 0;
	if(!dirs){
		char *bdir;
		for(bdir=map.basedirs;bdir && bdir[0];bdir+=FILELEN*2){
			struct maploaddir *ndir=malloc(sizeof(struct maploaddir));
			snprintf(ndir->dir,FILELEN,bdir);
			ndir->nxt=dirs;
			dirs=ndir;
		}
	}
	if(dirs) mapfindgps(&dirs);
	if(!dirs) map.init=MI_ALL;
	actadd(ACT_MAPCLT,NULL,NULL);
	return 1;
}

void mapaddbasedir(const char *dir,const char *name,char cfg){
	size_t ndir;
	char *bdir;
	if(!dir){ map.init=MI_BDIR; return; }
	if(!dir[0]) return;
	for(ndir=0,bdir=map.basedirs;bdir && bdir[0];bdir+=FILELEN*2) ndir++;
	map.basedirs=realloc(map.basedirs,(ndir+1)*FILELEN*2+1);
	snprintf(map.basedirs+ndir*FILELEN*2,FILELEN,dir);
	snprintf(map.basedirs+ndir*FILELEN*2+FILELEN,FILELEN-1,name);
	map.basedirs[(ndir+1)*FILELEN*2-1]=cfg;
	map.basedirs[(ndir+1)*FILELEN*2]='\0';
}

void mapinit(){
	while(map.init>2) SDL_Delay(500);
	map.starpat=cfggetstr("map.star");
	mapaddbasedir(cfggetstr("map.base"),"",1);
	map.ftchk=cfggetuint("ld.filetime_check");
	memset(&mapimgs,0,sizeof(struct mapimgs));
	texloadput(map.imgdir+MID_DIR,     IMG_Load(finddatafile("mapdir.png"     )));
	texloadput(map.imgdir+MID_DIRS,    IMG_Load(finddatafile("mapdirs.png"    )));
	texloadput(map.imgdir+MID_STAR,    IMG_Load(finddatafile("mapstar.png"    )));
	texloadput(map.imgdir+MID_STARDIRS,IMG_Load(finddatafile("mapstardirs.png")));
	texloadput(map.imgdir+MID_STARS,   IMG_Load(finddatafile("mapstars.png"   )));
	map.init=MI_TEX;
	mapeleinit(mapldinit());
}

void maprenderclt(){
	struct mapclti *clti;
	double mx,my;
	GLuint name=IMGI_MAP+1;
	int iz;
	struct ecur *ecur=mapecur(&iz,NULL);
	float s;
	if(!ecur || !map.scr_w || !map.scr_h) return;
	s=powf(2.f,ecur->s-(float)iz);
	mapg2m(ecur->x,ecur->y,iz,mx,my);
	for(clti=mapimgs.clt[iz].clts;clti;clti=clti->nxtclt){
		glLoadName(name++);
		if(optx){
		double k[3],xs=15./512.,ys=10./512.;
		glBindTexture(GL_TEXTURE_2D,map.imgdir[clti->id].tex);
		glBegin(GL_QUADS);
		glTexCoord2f(0.,0.); mapm2k(clti->mx-xs,clti->my-ys,iz,k); glVertex3dv(k);
		glTexCoord2f(1.,0.); mapm2k(clti->mx+xs,clti->my-ys,iz,k); glVertex3dv(k);
		glTexCoord2f(1.,1.); mapm2k(clti->mx+xs,clti->my+ys,iz,k); glVertex3dv(k);
		glTexCoord2f(0.,1.); mapm2k(clti->mx-xs,clti->my+ys,iz,k); glVertex3dv(k);
		glEnd();
		}else{
		glPushMatrix();
		glScalef(s*256.f/(float)*map.scr_w,s*256.f/(float)*map.scr_h,1.f);
		glTranslated(clti->mx-mx,clti->my-my,0.);
		glScalef(15.f/256.f/s,10.f/256.f/s,1.f);
		glBindTexture(GL_TEXTURE_2D,map.imgdir[clti->id].tex);
		glBegin(GL_QUADS);
		glTexCoord2f( 0.0, 0.0); glVertex2f(-0.5,-0.5);
		glTexCoord2f( 1.0, 0.0); glVertex2f( 0.5,-0.5);
		glTexCoord2f( 1.0, 1.0); glVertex2f( 0.5, 0.5);
		glTexCoord2f( 0.0, 1.0); glVertex2f(-0.5, 0.5);
		glEnd();
		glPopMatrix();
		}
	}
	glLoadName(0);
}

void maprendertile(int ix,int iy,int iz,int izo){
	struct tile *ti;
	float tx0=0.f,tx1=1.f,ty0=0.f,ty1=1.f;
	double k[12];
	if(ix<0 || iy<0 || ix>=(1<<iz) || iy>=(1<<iz)) return;
	if(optx && iz<5){
		maprendertile(ix*2+0,iy*2+0,iz+1,izo);
		maprendertile(ix*2+1,iy*2+0,iz+1,izo);
		maprendertile(ix*2+0,iy*2+1,iz+1,izo);
		maprendertile(ix*2+1,iy*2+1,iz+1,izo);
		return;
	}
	if(optx){
		mapm2k(ix+0,iy+0,iz,k+0); 
		mapm2k(ix+1,iy+0,iz,k+3);
		mapm2k(ix+1,iy+1,iz,k+6);
		mapm2k(ix+0,iy+1,iz,k+9);
	}
	while(iz>izo || !(ti=maptilefind(ix,iy,iz,0)) || !ti->tex){
		if(!iz--) return;
		tx0/=2.f; tx1/=2.f;
		ty0/=2.f; ty1/=2.f;
		if(ix%2){ tx0+=.5f; tx1+=.5f; }
		if(iy%2){ ty0+=.5f; ty1+=.5f; }
		ix/=2;
		iy/=2;
	}
	//printf("%i/%i: %i %i => %.2f %.2f\n",iz,izo,ix,iy,tx0,ty0);
	glBindTexture(GL_TEXTURE_2D,ti->tex);
	glBegin(GL_QUADS);
	if(optx){
	glTexCoord2f(tx0,ty0); glVertex3dv(k+0);
	glTexCoord2f(tx1,ty0); glVertex3dv(k+3);
	glTexCoord2f(tx1,ty1); glVertex3dv(k+6);
	glTexCoord2f(tx0,ty1); glVertex3dv(k+9);
	}else{
	glTexCoord2f(tx0,ty0); glVertex2f(-0.5,-0.5);
	glTexCoord2f(tx1,ty0); glVertex2f( 0.5,-0.5);
	glTexCoord2f(tx1,ty1); glVertex2f( 0.5, 0.5);
	glTexCoord2f(tx0,ty1); glVertex2f(-0.5, 0.5);
	}
	glEnd();
}

void maprendermap(){
	int ix,iy,iz;
	int iw,ih;
	float ox,oy;
	int ixc,iyc;
	float iscale;
	struct ecur *ecur=mapecur(&iz,&iscale);
	float s,r=0.f;
	if(!ecur || !mapscrtis(&iw,&ih)) return;
	s=powf(2.f,ecur->s-(float)iz);
	iw=(int)((float)iw*iscale);
	ih=(int)((float)ih*iscale);
	mapg2i(ecur->x,ecur->y,iz,ix,iy);
	mapg2o(ecur->x,ecur->y,iz,ox,oy);
	glPushMatrix();
//	glRotatef(90.f,0.f,0.f,1.f);
//	glScalef(16.f/9.f,9.f/16.f,0.f);
	if(optx){
//	glTranslated(0.f,0.f,*map.scr_h*M_PI/256./(1<<iz)/tan(35./2.)+1.f);
	r=0.19f*powf(2.f,ecur->s);
	glTranslatef(0.f,0.f,5.f);
	glScalef(r,-r,-r);
	glTranslatef(0.f,0.f,-1.f);
	glRotatef(ecur->y,1.f,0.f,0.f);
	glRotatef(ecur->x,0.f,-1.f,0.f);
	}else{
	glScalef(s*256.f/(float)*map.scr_w,s*256.f/(float)*map.scr_h,1.f);
	glTranslatef(ox,oy,0.f);
	glTranslatef((float)(-(iw-1)/2),(float)(-(ih-1)/2),0.f);
	}
	ix-=(iw-1)/2;
	iy-=(ih-1)/2;
	for(ixc=0;ixc<iw;ixc++){
		for(iyc=0;iyc<ih;iyc++){
			int pz=1<<iz, px=ix+ixc, py=iy+iyc;
			py=(py+2*pz)%(2*pz);
			if(py>=pz){ py=2*pz-1-py; px+=pz/2; }
			if(py<0){ py=-py; px+=pz/2; }
			px=(px+pz)%pz;
			maprendertile(px,py,iz,iz);
			if(!optx) glTranslatef(0.f,1.f,0.f);
		}
		if(!optx) glTranslatef(1.f,(float)-ih,0.f);
	}
	if(optx){
		r=(r+.001f)/r;
		glScalef(r,r,r);
		maprenderclt();
	}
	glPopMatrix();
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
	mapg2m(map.pos.gx,map.pos.gy,map.pos.iz,mx,my);
	sx=(clti->mx-mx)*256.f/(double)*map.scr_w;
	sy=(clti->my-my)*256.f/(double)*map.scr_h;
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
		nc=clti->nimg/nl;
		if(nl*nc<clti->nimg) nc++;
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
	if(map.init>MI_TEX || !mapon()) return 0;
	if(!sel) while(texload()) ;
	if(optx) glmodex(GLM_3D,35,0); else glmode(GLM_2D);
	if(glprg()) glColor4f(.5f,.5f,.5f,1.f);
	else        glColor4f(1.f,1.f,1.f,1.f);
	if(!sel) maprendermap();
	if(!sel && map.ele){
		double gsx0,gsx1,gsy0,gsy1,t;
		maps2g(-.5f,.0f,map.pos.iz,gsx0,t);
		maps2g( .5f,.0f,map.pos.iz,gsx1,t);
		maps2g(.0f, .5f,map.pos.iz,t,gsy0);
		maps2g(.0f,-.5f,map.pos.iz,t,gsy1);
		mapelerender(gsx0,gsx1,gsy0,gsy1);
	}
	if(!optx) maprenderclt();
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
	if(map.init>MI_TEX || !mapon()) return 0;
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
		maps2g(sx,sy,map.pos.iz,gx0,gy0);
		maps2g(sx,sy,iz,gx1,gy1);
		map.pos.iz=iz;
		map.pos.gx-=gx1-gx0;
		map.pos.gy-=gy1-gy0;
		debug(DBG_DBG,"map zoom %i (%s)",iz,maptypes[map.maptype].id);
	}
	sdlforceredraw();
	return 1;
}

/* thread: dpl */
char mapmovepos(float sx,float sy){
	double gx0,gx1,gy0,gy1;
	if(map.init>MI_TEX || !mapon()) return 0;
	maps2g(0.,0.,map.pos.iz,gx0,gy0);
	maps2g(sx,sy,map.pos.iz,gx1,gy1);
	map.pos.gx+=gx1-gx0;
	map.pos.gy+=gy1-gy0;
	return 1;
}

void mapimgsave(const char *dir){
	const char *name=mapimgname(dir);
	struct mapimg *imgs[N_DIRID];
	struct mapimg *i;
	int di;
	char buf[FILELEN];
	FILE *fd;
	snprintf(buf,FILELEN,"%s/gps.txt",dir);
	if(!(fd=fopen(buf,"w"))) return;
	memset(imgs,0,sizeof(struct mapimg *)*N_DIRID);
	for(i=mapimgs.img;i;i=i->nxt) if(!strncmp(i->name,name,FILELEN) && i->dirid>=0 && i->dirid<N_DIRID)
		imgs[i->dirid]=i;
	for(di=0;di<N_DIRID;di++) if(imgs[di])
		fprintf(fd,"%.6f,%.6f\n",imgs[di]->gy,imgs[di]->gx);
	fclose(fd);
}

/* thread: dpl */
char mapmarkpos(float sx,float sy,const char *dir){
	double gx,gy;
	if(map.init>MI_ALL || !mapon()) return 0;
	if(!(filetype(dir)&FT_DIR)) return 0;
	maps2g(sx,sy,map.pos.iz,gx,gy);
	mapimgadd(dir,-1,gx,gy,1);
	mapimgsave(dir);
	return 1;
}

void mapsearch(struct dplinput *in){
	struct mapimg *img,*imgf=NULL;
	size_t pf=1000,ilen=strlen(in->in);
	int id;
	if(map.init>MI_ALL || !mapon()) return;
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
			ileffref(NULL,EFFREF_ALL);
		}
	}
}

/* thread: eff */
struct mappos *mapgetpos(){
	if(map.init>MI_TEX || !mapon()) return NULL;
	return &map.pos;
}

void mapsavepos(){
	if(map.init>MI_TEX || !mapon()) return;
	map.possave=map.pos;
}

void maprestorepos(){
	if(map.init>MI_TEX || !mapon()) return;
	map.pos=map.possave;
	ileffref(NULL,EFFREF_ALL);
}

char mapstatupdate(char *dsttxt){
	double gsx0,gsx1,gsy0,gsy1,t;
	char fmt[128];
	char *txt;
	if(map.init>MI_TEX || !mapon()) return 0;
	maps2g( .5f,.0f,map.pos.iz,gsx0,t);
	maps2g(-.5f,.0f,map.pos.iz,gsx1,t);
	maps2g(.0f, .5f,map.pos.iz,t,gsy0);
	maps2g(.0f,-.5f,map.pos.iz,t,gsy1);
	gsx0=fabs(gsx1-gsx0);
	gsy0=fabs(gsy1-gsy0);
	gsx0*=(6378.137*2.*M_PI)/360.*cos(map.pos.gy/180.*M_PI);
	gsy0*=(6356.752314*2.*M_PI)/360.;
	snprintf(fmt,128,"%%.3f%%c %%.3f%%c %%.%ifx%%.%ifkm",gsx0>20.?0:gsx0>5.?1:2,gsy0>20.?0:gsy0>5.?1:2);
	snprintf(dsttxt,ISTAT_TXTSIZE,fmt,
			 fabs(map.pos.gy),map.pos.gy<0?'S':'N',
			 fabs(map.pos.gx),map.pos.gx<0?'W':'E',
			 gsx0,gsy0);
	txt=dsttxt+strlen(dsttxt);
	if(dplwritemode())
		snprintf(txt,(size_t)(dsttxt+ISTAT_TXTSIZE-txt),"%s [%s]",
			 _(" (write-mode)"),
			 (map.editmode==MEM_ADD?_("Add"):_("Replace")));
	return 1;
}

void mapcltmove(int i,float sx,float sy){
	struct mapclti *clti;
	float csx,csy;
	if(map.init>MI_ALL || !mapon()) return;
	for(clti=mapimgs.clt[map.pos.iz].clts;clti && i>0;clti=clti->nxtclt) i--;
	if(!clti) return;
	mapm2s(clti->mx,clti->my,map.pos.iz,csx,csy);
	csx-=sx;
	csy-=sy;
	maps2m(csx,csy,map.pos.iz,clti->mx,clti->my);
	sdlforceredraw();
}

char mapcltsave(int i){
	struct mapclti *clti,*ci;
	double mx=0.,my=0.;
	if(map.init>MI_ALL || !mapon()) return 0;
	for(clti=mapimgs.clt[map.pos.iz].clts;clti && i>0;clti=clti->nxtclt) i--;
	for(ci=clti;ci;ci=ci->nxtimg){
		double cmx,cmy;
		mapg2m(ci->img->gx,ci->img->gy,map.pos.iz,cmx,cmy);
		mx+=cmx; my+=cmy;
	}
	mx/=(double)clti->nimg; my/=(double)clti->nimg;
	mx-=clti->mx; my-=clti->my;
	for(ci=clti;ci;ci=ci->nxtimg){
		double cmx,cmy;
		mapg2m(ci->img->gx,ci->img->gy,map.pos.iz,cmx,cmy);
		cmx-=mx; cmy-=my;
		mapm2g(cmx,cmy,map.pos.iz,ci->img->gx,ci->img->gy);
		mapimgsave(ci->img->dir);
	}
	actadd(ACT_MAPCLT,NULL,NULL);
	return 1;
}

