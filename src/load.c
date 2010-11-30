#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <SDL.h>
#include <SDL_image.h>
#if HAVE_REALPATH
	#include <sys/param.h>
	#include <unistd.h>
#endif
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
#include "ldjpg.h"

#ifndef popen
	extern FILE *popen (__const char *__command, __const char *__modes);
	extern int pclose (FILE *__stream);
#endif

/***************************** load *******************************************/

struct load {
	int  minimgslim[TEX_NUM];
	int  maximgwide[TEX_NUM];
	int  maxtexsize;
	int  maxpanotexsize;
	int  maxpanopixels;
	char vartex;
} load = {
	.minimgslim = { 128, 384, 1024,    0, },
	.maximgwide = { 512, 512, 2048, 8192, },
	.vartex = 0,
};

/* thread: gl */
void ldmaxtexsize(){
	GLint maxtex;
	load.maxtexsize=cfggetint("ld.maxtexsize");
	load.maxpanotexsize=cfggetint("ld.maxpanotexsize");
	load.maxpanopixels=cfggetint("ld.maxpanopixels");
	glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maxtex);
	if(maxtex<load.maxtexsize) load.maxtexsize=maxtex;
	if(maxtex<load.maxpanotexsize) load.maxpanotexsize=maxtex;
}

/* thread: gl */
#define VT_W	67
#define VT_H	75
void ldcheckvartex(){
	GLuint tex=0;
	char dat[3*VT_W*VT_H];
	glGenTextures(1,&tex);
	glBindTexture(GL_TEXTURE_2D,tex);
	glTexImage2D(GL_TEXTURE_2D,0,3,VT_W,VT_H,0,GL_RGB,GL_UNSIGNED_BYTE,dat);
	glDeleteTextures(1,&tex);
	load.vartex=!glGetError();
}

/***************************** imgld ******************************************/

struct itex {
	struct itx *tx;
	GLuint dl;
	GLuint dlpano;
	char loaded;
	char loading;
	char thumb;
	char refresh;
	struct ipano *pano;
};

struct imgld {
	char fn[FILELEN];
	char tfn[FILELEN];
	char loadfail;
	int w,h;
	struct itex texs[TEX_NUM];
	struct img *img;
	struct ipano pano;
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
	if(it==TEX_PANO) return il->texs[TEX_FULL].pano &&
				il->texs[TEX_FULL].loaded && il->texs[TEX_FULL].loading ?
			il->texs[TEX_FULL].dlpano : 0;
	for(i=it;i>=0;i--) if(il->texs[i].loaded && il->texs[i].loading) return il->texs[i].dl;
	for(i=it;i<TEX_NUM;i++) if(il->texs[i].loaded && il->texs[i].loading) return il->texs[i].dl;
	return 0;
}

/* thread: dpl, load, gl */
float imgldrat(struct imgld *il){
	return (!il->h || !il->w) ? 0. : (float)il->w/(float)il->h;
}

/* thread: dpl, load */
char *imgldfn(struct imgld *il){ return il->fn; }

void imgldrefresh(struct imgld *il){
	int i;
	for(i=0;i<TEX_NUM;i++) il->texs[i].refresh=1;
}

/* thread: gl */
struct ipano *imgldpano(struct imgld *il){ return il->pano.enable ? &il->pano : NULL; }

/***************************** sdlimg *****************************************/

struct sdlimg {
	SDL_Surface *sf;
	int ref;
};

void sdlimg_unref(struct sdlimg *sdlimg){
	if(!sdlimg) return;
	if(--sdlimg->ref) return;
	SDL_FreeSurface(sdlimg->sf);
	free(sdlimg);
}

void sdlimg_ref(struct sdlimg *sdlimg){
	if(!sdlimg) return;
	sdlimg->ref++;
}

struct sdlimg* sdlimg_gen(SDL_Surface *sf){
	struct sdlimg *sdlimg;
	if(!sf) return NULL;
	sdlimg=malloc(sizeof(struct sdlimg));
	sdlimg->sf=sf;
	sdlimg->ref=1;
	return sdlimg;
}

/***************************** texload ****************************************/

struct texload {
	struct itx *itx;
	struct sdlimg *sdlimg;
	struct itex *itex;
	float bar;
};
#define TEXLOADNUM	16
struct texloadbuf {
	struct texload tl[TEXLOADNUM];
	int wi,ri;
} tlb = {
	.wi=0,
	.ri=0,
};
void ldtexload_put(struct itx *itx,struct sdlimg *sdlimg,struct itex *itex,float bar){
	int nwi=(tlb.wi+1)%TEXLOADNUM;
	while(nwi==tlb.ri){
		if(sdl_quit) return;
		SDL_Delay(10);
	}
	tlb.tl[tlb.wi].itx=itx;
	tlb.tl[tlb.wi].sdlimg=sdlimg;
	tlb.tl[tlb.wi].itex=itex;
	tlb.tl[tlb.wi].bar=bar;
	tlb.wi=nwi;
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
	if(tl->sdlimg){
		if(dplineff() && (tl->sdlimg->sf->w>=1024 || tl->sdlimg->sf->h>=1024)) return 0;
		timer(TI_LD,-1,0);
		if(!tl->itx->tex) glGenTextures(1,&tl->itx->tex);
		glBindTexture(GL_TEXTURE_2D, tl->itx->tex);
		// http://www.opengl.org/discussion_boards/ubbthreads.php?ubb=showflat&Number=256344
		// http://www.songho.ca/opengl/gl_pbo.html
		glTexImage2D(GL_TEXTURE_2D, 0, 3, tl->sdlimg->sf->w, tl->sdlimg->sf->h, 0, GL_RGB, GL_UNSIGNED_BYTE, tl->sdlimg->sf->pixels);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		//glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP);
		timer(TI_LD,MAX(tl->sdlimg->sf->w,tl->sdlimg->sf->h)/256,0);
		sdlimg_unref(tl->sdlimg);
		timer(TI_LD,0,0);
		if(tl->itex){
			ldgendl(tl->itex);
			tl->itex->loaded=1;
		}
		glsetbar(tl->bar);
	}else{
		if(tl->itx->tex) glDeleteTextures(1,&tl->itx->tex);
		tl->itx->tex=0;
		if(tl->itex) tl->itex->loaded=0;
	}
	if((glerr=glGetError())){
		error(ERR_CONT,"glTexImage2D %s failed (gl-err: %d)",tl->sdlimg?"load":"free",glerr);
		if(tl->sdlimg) tl->itx->tex=0;
	}
	tlb.ri=(tlb.ri+1)%TEXLOADNUM;
	return 1;
}

/***************************** load + free img ********************************/

char ldfload(struct imgld *il,enum imgtex it){
	struct sdlimg *sdlimg;
	int i;
	char ld=0;
	char *fn = il->fn;
	char thumb=0;
	struct icol *icol;
	int wide,slim;
	int lastscale=0;
	char swap=0;
	if(il->loadfail) return ld;
	if(it<0) return ld;
	if(il->texs[it].loading != il->texs[it].loaded) return ld;
	if(il->texs[it].loaded && !il->texs[it].refresh) return ld;
	imgexifload(il->img->exif,il->fn);
	if(it<TEX_BIG && il->tfn[0]){ fn=il->tfn; thumb=1; }
	debug(DBG_DBG,"ld loading img tex %s %s",imgtex_str[it],fn);
	if(it==TEX_FULL && il->pano.enable) glsetbar(0.0001f);
	sdlimg=sdlimg_gen(IMG_Load(fn));
	if(!sdlimg){ swap=1; sdlimg=sdlimg_gen(JPG_LoadSwap(fn)); }
	if(!sdlimg){ error(ERR_CONT,"Loading img failed \"%s\": %s",fn,IMG_GetError()); goto end; }
	if(sdlimg->sf->format->BytesPerPixel!=3){ error(ERR_CONT,"Wrong pixelformat \"%s\"",fn); goto end; }

	icol=imgposcol(il->img->pos);
	if(icol->g) SDL_ColMod(sdlimg->sf,icol->g,0.f,0.f);

	if(!swap){
		il->w=sdlimg->sf->w;
		il->h=sdlimg->sf->h;
	}else{
		il->w=sdlimg->sf->h;
		il->h=sdlimg->sf->w;
	}
	effrefresh(EFFREF_FIT);
	if(il->w<il->h){ slim=il->w; wide=il->h; }
	else           { slim=il->h; wide=il->w; }

	for(i=0;i<=it;i++){
		int scale=1;
		float sw,sh;
		int xres=load.maxtexsize;
		int yres=load.maxtexsize;
		int tw,th,tx,ty;
		struct itex *tex = il->texs+i;
		struct itx *ti;
		if(tex->loaded && !tex->refresh && (thumb || i!=TEX_SMALL || !tex->thumb)) continue;
		if(tex->loading != tex->loaded) continue;
		if(i==TEX_FULL && il->pano.enable){
			tex->pano=&il->pano;
			xres=load.maxpanotexsize;
			yres=load.maxpanotexsize;
			while(il->w/scale*il->h/scale>load.maxpanopixels) scale++;
			panores(il->img,&il->pano,il->w/scale,il->h/scale,&xres,&yres);
		}else tex->pano=NULL;
		tex->loading=1;
		tex->loaded=0;
		if(i!=TEX_FULL){
			while(slim/(scale+1)>=load.minimgslim[i]) scale++;
			while(wide/scale>load.maximgwide[i]) scale++;
		}
		if(lastscale && lastscale==scale && !tex->pano) continue;
		lastscale=scale;
		sw=il->w/scale;
		sh=il->h/scale;
		tw=sw/xres; if(tw*xres<sw) tw++;
		th=sh/yres; if(th*yres<sh) th++;
		debug(DBG_STA,"ld Loading to tex %s (%ix%i -> %i -> %ix%i -> t: %ix%i %ix%i)",imgtex_str[i],il->w,il->h,scale,il->w/scale,il->h/scale,tw,th,xres,yres);
		tx=0;
		if(tex->tx){
			while(tex->tx[tx].tex) tx++;
			while(tx>tw*th){
				tx--;
				/* TODO: glDeleteTextures(tex->tx[tx] */
			}
		}
		ti=tex->tx=realloc(tex->tx,sizeof(struct itx)*(tw*th+1));
		for(;tx<=tw*th;tx++) tex->tx[tx].tex=0;
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
			ldtexload_put(ti,sdlimgscale,
					tx==tw-1 && ty==th-1 ? tex : NULL,
					(tx!=tw-1 || ty!=th-1) && i==TEX_FULL && il->pano.enable ? (float)(tx*th+ty+1)/(float)(tw*th) : 0.f
				);
		}
		tex->thumb=thumb;
		tex->refresh=0;
		ld=1;
	}
	sdlimg_unref(sdlimg);
	return ld;
end:
	sdlimg_unref(sdlimg);
	il->loadfail=1;
	return ld;
}

char ldffree(struct imgld *il,enum imgtex thold){
	int it;
	char ret=0;
	struct itx *tx;
	for(it=thold+1;it<TEX_NUM;it++) if(il->texs[it].loaded){
		if(il->texs[it].loading != il->texs[it].loaded) continue;
		il->texs[it].loading=0;
		debug(DBG_STA,"ld freeing img tex %s %s",imgtex_str[it],il->fn);
		for(tx=il->texs[it].tx;tx && tx->tex;tx++) ldtexload_put(tx,NULL,!tx[1].tex ? il->texs+it : NULL,0.f);
		ret=1;
	}
	return ret;
}

/***************************** load concept *************************************/

struct loadconcept {
	char *load_str;
	char *hold_str;
	struct loadconept_ld {
		int imgi;
		enum imgtex tex;
	} *load;
	enum imgtex *hold;
	int hold_min;
	int hold_max;
};

struct loadconcept loadconcepts[] = {
	{ "F:0,B:0..2,-1;S:+-5,+-10,3..4,-2..-4", "F:0,B:-2..4;S:..15;T:..24" }, /* zoom >  0 */
	{ "B:0..2,-1;S:+-5,+-10,3..4,-2..-4",     "B:-2..4;S:..15;T:..24"     }, /* zoom =  0 */
	{ "S:..17;B:0,1",                         "B:-1..2;S:..22;T:..38"     }, /* zoom = -1 */
	{ "S:..22;T:+-23..31;B:0",                "B:..1;S:..37;T:..52"       }, /* zoom = -2 */
	{ "T:..38;S:..22;B:0",                    "B:..1;S:..37;T:..59"       }, /* zoom = -3 */
};
#define NUM_CONCEPTS (sizeof(loadconcepts)/sizeof(struct loadconcept))

struct loadconept_ld *ldconceptadd(int imgi,enum imgtex tex){
	static int num=0,size=0;
	static struct loadconept_ld *ret=NULL;

	if(num==size) ret=realloc(ret,sizeof(struct loadconept_ld)*(size+=32));
	ret[num].imgi=imgi;
	ret[num].tex=tex;
	num++;

	if(tex==TEX_NONE){
		struct loadconept_ld *retl=ret;
		num=size=0;
		ret=NULL;
		return retl;
	}else return NULL;
}

struct loadconept_ld *ldconceptstr(char *str){
	char strcopy[1024];
	char *tok;
	enum imgtex tex=TEX_NONE;
	snprintf(strcopy,1024,str);
	str=strcopy;
	while((tok=strsep(&str,",;"))){
		char sign=0;
		int  istr,iend;
		char *tokend;
		if(tok[1]==':'){
			switch(tok[0]){
				case 'F': tex=TEX_FULL; break;
				case 'B': tex=TEX_BIG; break;
				case 'S': tex=TEX_SMALL; break;
				case 'T': tex=TEX_TINY; break;
				default: error(ERR_QUIT,"concept: unknown tex '%c'",tok[0]);
			}
			tok+=2;
		}
		if(tok[0]=='.' && tok[1]=='.'){
			iend=strtol(tok+2,&tokend,0);
			if(tokend==tok+2) error(ERR_QUIT,"concept: no digits after '..': '%s'",tokend);
			if(tokend[0]) error(ERR_QUIT,"concept: str after '..[0-9]+': '%s'",tokend);
			if(iend<0) iend*=-1;
			for(istr=0;istr<=iend;istr++){
				ldconceptadd(istr,tex);
				ldconceptadd(-istr,tex);
			}
			continue;
		}
		if(tok[0]=='+' && tok[1]=='-'){ sign=1; tok+=2; }
		istr=strtol(tok,&tokend,0);
		if(tokend==tok) error(ERR_QUIT,"concept: no digits at begin: '%s'",tokend);
		if(tokend[-1]=='.') tokend--;
		tok=tokend;
		if(tok[0]=='.' && tok[1]=='.'){
			iend=strtol(tok+2,&tokend,0);
			if(tokend==tok+2) error(ERR_QUIT,"concept: no digits after '[0-9]+..': '%s'",tokend);
			if(tokend[0]) error(ERR_QUIT,"concept: str after '[0-9]+..[0-9]+': '%s'",tokend);
		}else{
			if(tok[0]) error(ERR_QUIT,"concept: str after '[0-9]+': '%s' (%s)",tok,strcopy);
			iend=istr;
		}
		while(1){
			ldconceptadd(istr,tex);
			if(sign) ldconceptadd(-istr,tex);
			if(istr==iend) break;
			istr += istr<iend ? 1 : -1;
		}
	}
	return ldconceptadd(0,TEX_NONE);
}

void ldconceptcompile(){
	int z;
	for(z=0;z<NUM_CONCEPTS;z++){
		int i,hmin,hmax;
		struct loadconept_ld *hold;
		loadconcepts[z].load = ldconceptstr(loadconcepts[z].load_str);
		hold = ldconceptstr(loadconcepts[z].hold_str);
		for(hmin=hmax=i=0;hold[i].tex!=TEX_NONE;i++){
			if(hold[i].imgi<hmin) hmin=hold[i].imgi;
			if(hold[i].imgi>hmax) hmax=hold[i].imgi;
		}
		loadconcepts[z].hold_min = hmin;
		loadconcepts[z].hold_max = hmax;
		loadconcepts[z].hold=malloc(sizeof(enum imgtex)*(hmax-hmin+1));
		memset(loadconcepts[z].hold,-1,sizeof(enum imgtex)*(hmax-hmin+1));
		for(i=0;hold[i].tex!=TEX_NONE;i++) if(loadconcepts[z].hold[hold[i].imgi-hmin]<hold[i].tex)
			loadconcepts[z].hold[hold[i].imgi-hmin]=hold[i].tex;
		free(hold);
	}
}

void ldconceptfree(){
	int z;
	for(z=0;z<NUM_CONCEPTS;z++){
		free(loadconcepts[z].load);
		free(loadconcepts[z].hold);
	}
}

/***************************** load check *************************************/

char ldcheck(){
	int i;
	int imgi = dplgetimgi();
	int zoom = dplgetzoom();
	struct loadconcept *ldcp;
	char ret=0;

	if(zoom>0) ldcp=loadconcepts+0;
	else if(zoom==0) ldcp=loadconcepts+1;
	else if(1-zoom<NUM_CONCEPTS) ldcp=loadconcepts+(1-zoom);
	else ldcp=loadconcepts+NUM_CONCEPTS-1;

	for(i=0;i<nimg;i++){
		enum imgtex hold=TEX_NONE;
		int diff=i-imgi;
		if(dplloop()){
			while(diff>nimg/2) diff-=nimg;
			while(diff<-nimg/2) diff+=nimg;
		}
		if(diff>=ldcp->hold_min && diff<=ldcp->hold_max)
			hold=ldcp->hold[diff-ldcp->hold_min];
		if(ldffree(imgs[i]->ld,hold)){ ret=1; break; }
	}

	for(i=0;ldcp->load[i].tex!=TEX_NONE;i++){
		int imgri;
		struct img *img;
		enum imgtex tex;
		imgri=imgi+ldcp->load[i].imgi;
		if(dplloop()) imgri=(imgri+nimg)%nimg;
		img=imgget(imgri);
		tex=ldcp->load[i].tex;
		if(img && ldfload(img->ld,tex)){ ret=1; break; }
	}

	return ret;
}

/***************************** load thread ************************************/

extern Uint32 paint_last;

int ldthread(void *arg){
	ldconceptcompile();
	ldfload(defimg->ld,TEX_BIG);
	while(!sdl_quit){
		if(!ldcheck()) SDL_Delay(100); else if(dplineff()) SDL_Delay(20);
		sdlthreadcheck();
	}
	ldconceptfree();
	sdl_quit|=THR_LD;
	return 0;
}

/***************************** load files init ********************************/

char ldfindfilesubdir1(char *dst,int len,char *subdir,char *ext){
	int i;
	FILE *fd;
	char fn[FILELEN];
	char *extpos = ext ? strrchr(dst,'.') : NULL;
	if(extpos>dst+4 && !strncmp(extpos-4,"_cut",4)) extpos-=4;
	if(extpos>dst+6 && !strncmp(extpos-6,"_small",6)) extpos-=6;
	if(extpos) extpos[0]='\0';
	for(i=strlen(dst)-1;i>=0;i--) if(dst[i]=='/'){
		dst[i]='\0';
		snprintf(fn,FILELEN,"%s/%s/%s%s",dst,subdir,dst+i+1,ext?ext:"");
		dst[i]='/';
		if((fd=fopen(fn,"rb"))){
			fclose(fd);
			strncpy(dst,fn,len);
			return 1;
		}
		if(subdir[0]=='\0') break;
	}
	if(extpos) extpos[0]='.';
	return 0;
}

char ldfindfilesubdir(char *dst,char *subdir,char *ext){
	if(ldfindfilesubdir1(dst,FILELEN,subdir,ext)) return 1;
#if HAVE_REALPATH
	{
		static char rfn[MAXPATHLEN];
		if(realpath(dst,rfn) && ldfindfilesubdir1(rfn,MAXPATHLEN,subdir,ext)){
			strncpy(dst,rfn,FILELEN);
			return 1;
		}
	}
#endif
	return 0;
}

void ldpanoinit(struct imgld *il){
	char fn[FILELEN];
	FILE *fd;
	il->pano.rotinit=4.f;
	il->pano.gw=il->pano.gh=0.f;
	strncpy(fn,il->fn,FILELEN);
	if(!ldfindfilesubdir(fn,"ori",".pano") && !ldfindfilesubdir(fn,"",".pano")) goto end;
	if(!(fd=fopen(fn,"r"))) goto end;
	fscanf(fd,"%f %f %f %f",&il->pano.gw,&il->pano.gh,&il->pano.gyoff,&il->pano.rotinit);
	fclose(fd);
end:
	if((il->pano.enable = il->pano.gw && il->pano.gh))
		debug(DBG_STA,"panoinit pano used: '%s'",fn);
	else
		debug(DBG_DBG,"panoinit no pano found for '%s'",il->fn);
}

void ldthumbinit(struct imgld *il){
	strncpy(il->tfn,il->fn,FILELEN);
	if(!ldfindfilesubdir(il->tfn,"thumb",NULL)){
		il->tfn[0]='\0';
		debug(DBG_DBG,"thumbinit no thumb found for '%s'",il->fn);
	}else
		debug(DBG_DBG,"thumbinit thumb used: '%s'",il->tfn);
}

void ldaddfile(char *fn){
	struct img *img=imgadd();
	if(!strncmp(fn,"file://",7)) fn+=7;
	strncpy(img->ld->fn,fn,FILELEN);
	ldthumbinit(img->ld);
	ldpanoinit(img->ld);
}

void ldaddflst(char *flst){
	FILE *fd=fopen(flst,"r");
	char buf[FILELEN];
	if(!fd){ error(ERR_CONT,"ld read flst failed \"%s\"",flst); return; }
	while(!feof(fd)){
		int len;
		if(!fgets(buf,FILELEN,fd)) continue;
		len=strlen(buf);
		while(buf[len-1]=='\n' || buf[len-1]=='\r') buf[--len]='\0';
		ldaddfile(buf);
	}
	fclose(fd);
}

void ldgetfiles(int argc,char **argv){
	char *defimgfn = finddatafile("defimg.png");
	if(!defimgfn) defimgfn="";
	defimg=imginit();
	strncpy(defimg->ld->fn,defimgfn,FILELEN);
	for(;argc;argc--,argv++){
		if(!strcmp(".flst",argv[0]+strlen(argv[0])-5)) ldaddflst(argv[0]);
		else ldaddfile(argv[0]);
	}
	if(cfggetint("ld.random")) imgrandom();
	actadd(ACT_LOADMARKS,NULL);
}
