#include <stdlib.h>
#include <stdio.h>
#include <GL/glew.h>
#include <SDL.h>
#include <SDL_image.h>
#include <math.h>
#include "load.h"
#include "sdl.h"
#include "main.h"
#include "help.h"
#include "img.h"
#include "dpl.h"
#include "exif.h"
#include "cfg.h"
#include "act.h"
#include "gl.h"
#include "file.h"
#include "eff.h"
#include "ldcp.h"
#include "pano.h"
#include "prg.h"
#include "map.h"
#include "hist.h"
#include "sdlimg.h"
#include "ldc.h"

#define E2(X,N)	#X
const char *imgtex_str[]={ IMGTEX };
#undef E2

#ifndef popen
	extern FILE *popen (__const char *__command, __const char *__modes);
	extern int pclose (FILE *__stream);
#endif

/***************************** load *******************************************/

struct load {
	Uint32 ftchk;
	int  minimgslim[TEX_NUM];
	int  maximgwide[TEX_NUM];
	int  maxtexsize;
	int  maxpanotexsize;
	int  maxpanopixels;
	int numexifloadperimg;
	char vartex;
	char reset;
} load = {
	.minimgslim = { 160, 384, 1024,    0, },
	.maximgwide = { 512, 512, 2048, 8192, },
	.vartex = 0,
	.reset = 0,
};

/* thread: gl */
void ldmaxtexsize(){
	GLint maxtex;
	load.ftchk=cfggetuint("ld.filetime_check");
	load.maxtexsize=cfggetint("ld.maxtexsize");
	load.maxpanotexsize=cfggetint("ld.maxpanotexsize");
	load.maxpanopixels=cfggetint("ld.maxpanopixels");
	glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maxtex);
	if(maxtex<load.maxtexsize) load.maxtexsize=maxtex;
	if(maxtex<load.maxpanotexsize) load.maxpanotexsize=maxtex;
	load.numexifloadperimg=cfggetint("ld.numexifloadperimg");
}

/* thread: gl */
void ldcheckvartex(){
#ifndef __WIN32__
	#define VT_W	67
	#define VT_H	75
	GLuint tex=0;
	char dat[3*VT_W*VT_H];
	glGenTextures(1,&tex);
	glBindTexture(GL_TEXTURE_2D,tex);
	glTexImage2D(GL_TEXTURE_2D,0,3,VT_W,VT_H,0,GL_RGB,GL_UNSIGNED_BYTE,dat);
	glDeleteTextures(1,&tex);
	load.vartex=!glGetError();
#endif
}

/* thread: sdl */
void ldreset(){ load.reset=1; }

/***************************** imgld ******************************************/

struct itex {
	struct itx *tx;
	struct imgpano *pano;
	GLuint dl;
	GLuint dlpano;
	char loaded;
	char loading;
	char thumb;
};

struct imgld {
	char loadfail;
	int w,h;
	struct itex texs[TEX_NUM];
	struct img *img;
	struct ldft lf;
};

/* thread: img */
struct imgld *imgldinit(struct img *img){
	struct imgld *il=calloc(1,sizeof(struct imgld));
	il->img=img;
	return il;
}

/* thread: img */
void imgldfree(struct imgld *il){
	int i;
	struct itx *tx;
	for(i=0;i<TEX_NUM;i++){
		if(il->texs[i].dl)     glDeleteLists(il->texs[i].dl,1);
		if(il->texs[i].dlpano) glDeleteLists(il->texs[i].dlpano,1);
		if((tx=il->texs[i].tx)){
			for(;tx->tex;tx++) glDeleteTextures(1,&tx->tex);
			free(il->texs[i].tx);
		}
	}
	free(il);
}

/* thread: img */
void imgldsetimg(struct imgld *il,struct img *img){ il->img=img; }

/* thread: gl */
GLuint imgldtex(struct imgld *il,enum imgtex it){
	int i;
	if(imgfiledir(il->img->file)) il=dirimg->ld;
	if(il->loadfail) il=defimg->ld;
	if(it==TEX_PANO) return il->texs[TEX_FULL].pano &&
				il->texs[TEX_FULL].loaded && il->texs[TEX_FULL].loading ?
			il->texs[TEX_FULL].dlpano : 0;
	for(i=it;i>=0;i--) if(il->texs[i].loaded && il->texs[i].loading) return il->texs[i].dl;
	for(i=it;i<TEX_NUM;i++) if(il->texs[i].loaded && il->texs[i].loading) return il->texs[i].dl;
	return 0;
}

struct imgld *imgldget(struct imgld *il){
	if(!il) return NULL;
	if(imgfiletxt(il->img->file)) return NULL;
	if(imgfiledir(il->img->file)) return dirimg->ld;
	if(il->loadfail) return defimg->ld;
	return il;
}

/* thread: dpl, load, gl */
float imgldrat(struct imgld *il){
	if(!(il=imgldget(il))) return 1.f;
	if(!il->h || !il->w) return 0.f;
	return (float)il->w/(float)il->h;
}

/* thread: dpl */
char imgldwh(struct imgld *il,float *w,float *h){
	if(!(il=imgldget(il))) return 0;
	if(w) *w=(float)il->w;
	if(h) *h=(float)il->h;
	return 1;
}

/* thread: ld, act, dpl */
char ldfiletime(struct ldft *lf,enum eldft act,char *fn){
	char ret=0;
	switch(act){
	case FT_UPDATE:
		lf->ft=filetime(fn);
		lf->ftchk=SDL_GetTicks();
	break;
	case FT_RESET:
		lf->ft=0;
		lf->ftchk=0;
	break;
	case FT_CHECK:
	case FT_CHECKNOW: {
		Uint32 time=SDL_GetTicks();
		if(act==FT_CHECKNOW || lf->ftchk+load.ftchk<time){
			long ft=filetime(fn);
			if(ft>lf->ft){
				lf->ft=ft;
				ret=1;
			}
			lf->ftchk=time;
		}
	}
	}
	return ret;
}

char imgldfiletime(struct imgld *il,enum eldft act){
	return ldfiletime(&il->lf,act,imgfilefn(il->img->file));
}

/***************************** texload ****************************************/

#define TEXLOADMODE	E(IMG), E(FREE)
#define E(X)	#X
const char *texloadmodestr[]={TEXLOADMODE};
#undef E

#define E(X)	TLM_##X
struct texload {
	struct itx *itx;
	enum texloadmode {TEXLOADMODE} mode;
	union texloaddat {
		struct {
			struct sdlimg *sdlimg;
			struct itex *itex;
			float bar;
		} img;
		struct {
			struct itex *itex;
		} free;
	} dat;
};
#undef E
#define TEXLOADNUM	16
struct texloadbuf {
	struct texload tl[TEXLOADNUM];
	int wi,ri;
	long long wn,rn;
	SDL_mutex *pmx;
} tlb = {
	.wi=0,
	.ri=0,
	.wn=0,
	.rn=0,
	.pmx=NULL,
};
void ldtexload_put(struct itx *itx,struct sdlimg *sdlimg,struct itex *itex,float bar){
	int nwi;
	SDL_LockMutex(tlb.pmx);
	if(sdl_quit) goto end;
	nwi=(tlb.wi+1)%TEXLOADNUM;
	while(nwi==tlb.ri){
		if(sdl_quit) goto end;
		SDL_Delay(10);
	}
	tlb.tl[tlb.wi].itx=itx;
	if(sdlimg)  tlb.tl[tlb.wi].mode=TLM_IMG;
	else        tlb.tl[tlb.wi].mode=TLM_FREE;
	switch(tlb.tl[tlb.wi].mode){
	case TLM_IMG:
		tlb.tl[tlb.wi].dat.img.sdlimg=sdlimg;
		tlb.tl[tlb.wi].dat.img.itex=itex;
		tlb.tl[tlb.wi].dat.img.bar=bar;
	break;
	case TLM_FREE:
		tlb.tl[tlb.wi].dat.free.itex=itex;
	break;
	}
	tlb.wi=nwi;
	tlb.wn++;
end:
	SDL_UnlockMutex(tlb.pmx);
}

/* thread: sdl */
void ldgendl(struct itex *itex){
	if(!itex->dl) itex->dl=glGenLists(1);
	glNewList(itex->dl,GL_COMPILE);
	gldrawimg(itex->tx);
	glEndList();
	if(!itex->pano) return;
	if(!itex->dlpano) itex->dlpano=glGenLists(1);
	glNewList(itex->dlpano,GL_COMPILE);
	panodrawimg(itex->tx,itex->pano);
	glEndList();
}

/* thread: sdl */
char ldtexload(){
	GLenum glerr;
	struct texload *tl;
	if(tlb.wi==tlb.ri) return 0;
	tl=tlb.tl+tlb.ri;
	switch(tl->mode){
	case TLM_IMG: {
		struct sdlimg *sdlimg=tl->dat.img.sdlimg;
		if(!sdlimg->ref || !sdlimg->sf){ error(ERR_CONT,"ldtexload: load of free'd image"); break; }
		timer(TI_GLLD,-1,0);
		if(!tl->itx->tex) glGenTextures(1,&tl->itx->tex);
		glBindTexture(GL_TEXTURE_2D,tl->itx->tex);
		// http://www.opengl.org/discussion_boards/ubbthreads.php?ubb=showflat&Number=256344
		// http://www.songho.ca/opengl/gl_pbo.html
		glTexImage2D(GL_TEXTURE_2D,0,(GLint)sdlimg->fmt,sdlimg->sf->w,sdlimg->sf->h,0,sdlimg->fmt,GL_UNSIGNED_BYTE,sdlimg->sf->pixels);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
//		glGenerateMipmap(GL_TEXTURE_2D);
//		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
		//glTexEnvf(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_DECAL);
		if(GLEW_EXT_texture_edge_clamp){
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
		}else{
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP);
		}
		timer(TI_GLLD,MAX(sdlimg->sf->w,sdlimg->sf->h)/256,0);
		sdlimg_unref(sdlimg);
		timer(TI_GLLD,0,0);
		if(tl->dat.img.itex){
			ldgendl(tl->dat.img.itex);
			tl->dat.img.itex->loaded=1;
			tl->dat.img.itex->loading=1;
			sdlforceredraw();
		}
		glsetbar(tl->dat.img.bar);
	}
	break;
	case TLM_FREE:
		if(tl->itx->tex){
			glDeleteTextures(1,&tl->itx->tex);
			tl->itx->tex=0;
		}
		if(tl->dat.free.itex){
			tl->dat.free.itex->loaded=0;
			tl->dat.free.itex->loading=0;
		}
	break;
	}
	if((glerr=glGetError())){
		error(ERR_CONT,"glTexImage2D %s failed (gl-err: %d)",texloadmodestr[tl->mode],glerr);
		tl->itx->tex=0;
	}
	tlb.ri=(tlb.ri+1)%TEXLOADNUM;
	tlb.rn++;
	return 1;
}

GLuint ldfile2tex(const char *fn){
	struct sdlimg *sdlimg;
	GLuint tex;
	if(!fn) return 0;
	if(!(sdlimg=sdlimg_gen(IMG_Load(fn)))) return 0;
	glGenTextures(1,&tex);
	glBindTexture(GL_TEXTURE_2D,tex);
	glTexImage2D(GL_TEXTURE_2D,0,(GLint)sdlimg->fmt,sdlimg->sf->w,sdlimg->sf->h,0,sdlimg->fmt,GL_UNSIGNED_BYTE,sdlimg->sf->pixels);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	sdlimg_unref(sdlimg);
	return tex;
}

/***************************** load + free img ********************************/

enum imgtex ldcheck_texupd(enum imgtex *tex,struct img *img){
	struct loadconcept *ldcp;
	struct imglist *il=img->il;
	int i,cimgi;
	if(*tex!=TEX_BIG) return *tex;
	cimgi=ilrelimgi(il,ilcimgi(il));
	ldcp=ldconceptget();
	for(i=0;ldcp->load[i].tex==TEX_FULL;i++)
		if(ilclipimgi(il,cimgi+ldcp->load[i].imgi,0)==img->frm)
			return *tex=TEX_FULL;
	return *tex;
}

char ldfload(struct imgld *il,enum imgtex it){
	struct sdlimg *sdlimg;
	int i;
	char ld=0;
	const char *fn = imgfilefn(il->img->file);
	char thumb=0;
	int wide,slim;
	int lastscale=0;
	char swap=0;
	char panoenable=0;
	enum effrefresh effref=EFFREF_FIT;
	struct imglist *ilt=il->img->il?il->img->il:CIL_ALL;
	timer(TI_LDF,-1,0);
	if(imgfiledir(il->img->file)) goto end0;
	if(imgfiletxt(il->img->file)) goto end0;
	if(!strncmp(fn,"[MAP]",6)) goto end0;
	if(il->loadfail) goto end0;
	if(it<0){
		effref=EFFREF_NO;
		ld=imgexifload(il->img->exif,fn);
		ildelopt(il->img,ILO_EXIF);
		if(ld&0x2)  effref|=EFFREF_ROT;
		if(ld&0x4 && ilsortupd(il->img)) effref|=EFFREF_ALL;
		if(effref) ileffref(ilt,effref);
		ld&=0x1;
		goto end0;
	}
	if(il->texs[it].loading != il->texs[it].loaded) goto end0;
	if(il->texs[it].loaded){
		if(imgldfiletime(il,FT_CHECK)){
			fthumbchecktime(il->img->file);
			ldffree(il,TEX_NONE);
			imgexifclear(il->img->exif);
			ilsetopt(il->img,ILO_EXIF);
			imghistclear(il->img->hist);
		}
		goto end0;
	}
	ilsetopt(il->img,ILO_LOAD);
	imgldfiletime(il,FT_UPDATE);
	ld=imgexifload(il->img->exif,fn);
	ildelopt(il->img,ILO_EXIF);
	if(ld&0x2) effref|=EFFREF_ROT;
	if(ld&0x4 && ilsortupd(il->img)) effref|=EFFREF_ALL;
	if(it<TEX_BIG && imgfiletfn(il->img->file,&fn)) thumb=1;
	debug(DBG_STA,"ld loading img tex %s %s",_(imgtex_str[it]),fn);
	if(it==TEX_FULL && (panoenable=imgpanoenable(il->img->pano))) glsetbar(0.0001f);
	timer(TI_LDF,0,0);
	if(!(sdlimg=ldc(fn,thumb,&swap))){ error(ERR_CONT,"Loading img failed \"%s\": %s",fn,IMG_GetError()); goto end3; }
	if(!sdlimg->fmt){ error(ERR_CONT,"Not supported pixelformat \"%s\"",fn); goto end3; }
	timer(TI_LDF,1,0);

	if(!swap){
		il->w=sdlimg->sf->w;
		il->h=sdlimg->sf->h;
	}else{
		il->w=sdlimg->sf->h;
		il->h=sdlimg->sf->w;
	}
	ileffref(ilt,effref);
	if(il->w<il->h){ slim=il->w; wide=il->h; }
	else           { slim=il->h; wide=il->w; }
	timer(TI_LDF,2,0);
	imghistgen(il->img->hist,imgfilefn(il->img->file),il->w*il->h,
		sdlimg->fmt==GL_BGRA || sdlimg->fmt==GL_BGR,
		sdlimg->sf->format->BytesPerPixel,
		sdlimg->sf->pixels);
	timer(TI_LDF,3,0);

	for(i=0;i<=ldcheck_texupd(&it,il->img);i++){
		int scale=1;
		float sw,sh;
		int xres=load.maxtexsize;
		int yres=load.maxtexsize;
		int tw,th,tx,ty;
		struct itex *tex = il->texs+i;
		struct itx *ti;
		if(tex->loaded && (thumb || i!=TEX_SMALL || !tex->thumb)) continue;
		if(tex->loading != tex->loaded) continue;
		if(i==TEX_FULL && (panoenable=imgpanoenable(il->img->pano))){
			tex->pano=il->img->pano;
			xres=load.maxpanotexsize;
			yres=load.maxpanotexsize;
			while(il->w/scale*il->h/scale>load.maxpanopixels) scale++;
			panores(tex->pano,il->w/scale,il->h/scale,&xres,&yres);
		}else tex->pano=NULL;
		if(i!=TEX_FULL){
			while(slim/(scale+1)>=load.minimgslim[i]) scale++;
			while(wide/scale>load.maximgwide[i]) scale++;
		}
		if(lastscale && lastscale==scale && !tex->pano) continue;
		tex->loading=1;
		tex->loaded=0;
		lastscale=scale;
		sw=(float)(il->w/scale);
		sh=(float)(il->h/scale);
		while((float)(xres>>1)>=sw) xres>>=1;
		while((float)(yres>>1)>=sh) yres>>=1;
		tw=(int)ceil(sw/(float)xres);
		th=(int)ceil(sh/(float)yres);
		debug(DBG_DBG,"ld Loading to tex %s (%ix%i -> %i -> %ix%i -> t: %ix%i %ix%i)",_(imgtex_str[i]),il->w,il->h,scale,il->w/scale,il->h/scale,tw,th,xres,yres);
		tx=0;
		if(tex->tx){
			while(tex->tx[tx].tex) tx++;
			while(tx>tw*th){
				tx--;
				/* TODO: glDeleteTextures(tex->tx[tx] */
			}
		}
		ti=tex->tx=realloc(tex->tx,sizeof(struct itx)*(long unsigned int)(tw*th+1));
		memset(tex->tx+tx,0,sizeof(struct itx)*(long unsigned int)(tw*th+1-tx));
		for(tx=0;tx<tw;tx++) for(ty=0;ty<th;ty++,ti++){
			struct sdlimg *sdlimgscale;
			if(!(sdlimgscale = sdlimg_gen(SDL_ScaleSurfaceFactor(sdlimg->sf,scale,tx*xres,ty*yres,xres,yres,swap)))){
				sdlimgscale=sdlimg;
				sdlimg_ref(sdlimgscale);
			}
			ti->w=(float)sdlimgscale->sf->w/(float)sw;
			ti->h=(float)sdlimgscale->sf->h/(float)sh;
			ti->x=(float)(tx*xres)/(float)sw + ti->w*.5f - .5f;
			ti->y=(float)(ty*yres)/(float)sh + ti->h*.5f - .5f;
			if(!load.vartex){
				struct sdlimg *sdlimgscale2;
				int fw=1,fh=1;
				while(fw<sdlimgscale->sf->w) fw<<=1;
				while(fh<sdlimgscale->sf->h) fh<<=1;
				if((sdlimgscale2=sdlimg_gen(SDL_ScaleSurface(sdlimgscale->sf,fw,fh)))){
					sdlimg_unref(sdlimgscale);
					sdlimgscale=sdlimgscale2;
				}
			}
			if(!sdlimg->fmt) error(ERR_CONT,"Not supported pixelformat after scale \"%s\"",fn);
			else ldtexload_put(ti,sdlimgscale,
					tx==tw-1 && ty==th-1 ? tex : NULL,
					(tx!=tw-1 || ty!=th-1) && i==TEX_FULL && panoenable ? (float)(tx*th+ty+1)/(float)(tw*th) : 0.f
				);
		}
		tex->thumb=thumb;
		ld=127;
	}
	goto end2;
end3:
	il->loadfail=1;
end2:
	sdlimg_unref(sdlimg);
	timer(TI_LDF,4,0);
end0:
	return ld;
}

/* thread: load, dpl, act */
char ldffree(struct imgld *il,enum imgtex thold){
	int it;
	char ret=0;
	struct itx *tx;
	for(it=thold+1;it<TEX_NUM;it++) if(il->texs[it].loaded){
		if(il->texs[it].loading != il->texs[it].loaded) continue;
		il->texs[it].loading=0;
		debug(DBG_STA,"ld freeing img tex %s %s",_(imgtex_str[it]),imgfilefn(il->img->file));
		for(tx=il->texs[it].tx;tx && tx->tex;tx++) ldtexload_put(tx,NULL,!tx[1].tex ? il->texs+it : NULL,0.f);
		ret=1;
	}
	if(thold==TEX_NONE) ildelopt(il->img,ILO_LOAD);
	return ret;
}

/***************************** load check *************************************/

int ldcheckfree(struct img *img,void *arg){
	struct loadconcept *ldcp=(struct loadconcept*)arg;
	int cimgi=ilrelimgi(img->il,ilcimgi(img->il));
	enum imgtex hold=TEX_NONE;
	int diff=prgimgidiff(cimgi,img);
	char del = img->il==ilget(CIL_DEL) && !imgposcur(img->pos)->act;
#ifdef ILODEBUG
	char ld=0;
	int it;
	for(it=0;!ld && it<TEX_NUM;it++) if(img->ld->texs[it].loaded) ld=1;
	ilchkopt(img,ILO_LOAD,ld);
#endif
	if(diff>=ldcp->hold_min && diff<=ldcp->hold_max && !del)
		hold=ldcp->hold[diff-ldcp->hold_min];
	return ldffree(img->ld,hold);
}

int ldcheckexifload(struct img *img,void *UNUSED(arg)){
	return ldfload(img->ld,TEX_NONE);
}

char ldcheck(){
	int i,il;
	struct loadconcept *ldcp=ldconceptget();
	int ret=0;
	timer(TI_LD,-1,1);

	if(mapldcheck()) ret=1;
	timer(TI_LD,0,1);

	if(ilsforallimgs(ldcheckfree,ldcp,1,1,ILO_LOAD)) ret=1;
	timer(TI_LD,1,1);

	for(il=0;il<CIL_NUM;il++){
		int cimgi=ilrelimgi(CIL(il),ilcimgi(CIL(il)));
		for(i=0;ldcp->load[i].tex!=TEX_NONE;i++){
			int imgri;
			enum imgtex tex;
			imgri=ilclipimgi(CIL(il),cimgi+ldcp->load[i].imgi,0);
			tex=ldcp->load[i].tex;
			if(prgforoneldfrm(CIL(il),imgri,ldfload,tex)){ ret=1; break; }
		}
	}
	timer(TI_LD,2,1);

	if(ilsforallimgs(ldcheckexifload,NULL,1,load.numexifloadperimg,ILO_EXIF)) ret=1;
	timer(TI_LD,3,1);

	return ret!=0;
}

/***************************** load thread ************************************/

int ldresetdoimg(struct img *img,void *UNUSED(arg)){
	int it;
	struct itex *itex = img->ld->texs;
	for(it=0;it<TEX_NUM;it++) free(itex[it].tx);
	memset(itex,0,TEX_NUM*sizeof(struct itex));
	return 0;
}

void ldresetdo(){
	struct img *img=ilimg(CIL(0),0);
	if(!img) return;
	tlb.wi=tlb.ri; /* todo: cleanup texloadbuf */
	ldresetdoimg(defimg,NULL);
	ldresetdoimg(dirimg,NULL);
	ilsforallimgs(ldresetdoimg,NULL,0,0,ILO_ALL);
	ldfload(defimg->ld,TEX_BIG);
	ldfload(dirimg->ld,TEX_BIG);
	debug(DBG_STA,"ldreset done");
	load.reset=0;
}

extern Uint32 paint_last;

int ldthread(void *UNUSED(arg)){
	tlb.pmx=SDL_CreateMutex();
	ldconceptcompile();
	ldcinit();
	ldfload(defimg->ld,TEX_BIG);
	ldfload(dirimg->ld,TEX_BIG);
	while(!sdl_quit){
		if(!ldcheck()) SDL_Delay(100); else if(effineff()) SDL_Delay(20);
		if(load.reset) ldresetdo();
	}
	ldconceptfree();
	sdl_quit|=THR_LD;
	SDL_DestroyMutex(tlb.pmx); tlb.pmx=NULL;
	return 0;
}

