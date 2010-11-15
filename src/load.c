#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include <SDL_image.h>
#include "load.h"
#include "sdl.h"
#include "main.h"
#include "help.h"
#include "img.h"
#include "dpl.h"


void ldffree(struct imgld *il,enum imgtex it);

/***************************** load *******************************************/

struct load {
	char enable;
	int  texsize[TEX_NUM];
} load = {
	.enable = 0,
	.texsize = { 256, 512, 2048, 32768, }
};

void ldenable(char enable){ load.enable=enable; }
void ldmaxtexsize(GLint maxtexsize){
	int i;
	for(i=0;i<TEX_NUM;i++) if(load.texsize[i]>maxtexsize){
		if(i && load.texsize[i-1]>=maxtexsize) load.texsize[i]=0;
		else load.texsize[i]=maxtexsize;
	}
}

/***************************** imgld ******************************************/

struct itex {
	GLuint tex;
	char loading;
	char thumb;
};

struct imgld {
	char fn[1024];
	char tfn[1024];
	char loadfail;
	int w,h;
	float irat;
	struct itex texs[TEX_NUM];
	struct img *img;
};

struct imgld *imgldinit(struct img *img){
	struct imgld *il=calloc(1,sizeof(struct imgld));
	il->img=img;
	return il;
}

void imgldfree(struct imgld *il){
	int i;
	for(i=0;i<TEX_NUM;i++) if(il->texs[i].tex) glDeleteTextures(1,&il->texs[i].tex);
	free(il);
}

void imgldsetimg(struct imgld *il,struct img *img){ il->img=img; }

GLuint imgldtex(struct imgld *il,enum imgtex it){
	int i;
	for(i=it;i>=0;i--) if(il->texs[i].tex) return il->texs[i].tex;
	for(i=it;i<TEX_NUM;i++) if(il->texs[i].tex) return il->texs[i].tex;
	return 0;
}

float imgldrat(struct imgld *il){ return il->irat; }

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
	struct itex *itex;
	struct sdlimg *sdlimg;
};
#define TEXLOADNUM	16
struct texloadbuf {
	struct texload tl[TEXLOADNUM];
	int wi,ri;
} tlb = {
	.wi=0,
	.ri=0,
};
void ldtexload_put(struct itex *itex,struct sdlimg *sdlimg){
	int nwi=(tlb.wi+1)%TEXLOADNUM;
	while(nwi==tlb.ri) SDL_Delay(10);
	itex->loading=1;
	tlb.tl[tlb.wi].itex=itex;
	tlb.tl[tlb.wi].sdlimg=sdlimg;
	tlb.wi=nwi;
}

void ldtexload(){
	GLenum glerr;
	struct texload *tl;
	if(tlb.wi==tlb.ri) return;
	tl=tlb.tl+tlb.ri;
	if(tl->sdlimg){
		if(!tl->itex->tex) glGenTextures(1,&tl->itex->tex);
		glBindTexture(GL_TEXTURE_2D, tl->itex->tex);
		glTexImage2D(GL_TEXTURE_2D, 0, 3, tl->sdlimg->sf->w, tl->sdlimg->sf->h, 0, GL_RGB, GL_UNSIGNED_BYTE, tl->sdlimg->sf->pixels);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		sdlimg_unref(tl->sdlimg);
	}else{
		if(tl->itex->tex) glDeleteTextures(1,&tl->itex->tex);
		tl->itex->tex=0;
	}
	if((glerr=glGetError())){
		error(ERR_CONT,"glTexImage2D %s failed (gl-err: %d)",tl->sdlimg?"load":"free",glerr);
		if(tl->sdlimg) tl->itex->tex=0;
	}
	tl->itex->loading=0;
	tlb.ri=(tlb.ri+1)%TEXLOADNUM;
}

/***************************** load + free img ********************************/

int sizesel(int sdemand,int simg){
	int size=64;
	if(sdemand<simg) return sdemand;
	while(size<simg*0.95) size<<=1;
	return size;
}

char ldfload(struct imgld *il,enum imgtex it,char replace){
	struct sdlimg *sdlimg;
	int i;
	char ld=0;
	char *fn = il->fn;
	char thumb=0;
	if(il->loadfail) return ld;
	if(il->texs[it].loading) return ld;
	debug(DBG_STA,"ld loading img tex %s %s",imgtex_str[it],il->fn);

	if(it<TEX_BIG && il->tfn[0]){
		fn=il->tfn;
		thumb=1;
	}
	debug(DBG_DBG,"ld Loading img \"%s\"",fn);
	sdlimg=sdlimg_gen(IMG_Load(fn));
	if(!sdlimg){ error(ERR_CONT,"Loading img failed \"%s\": %s",fn,IMG_GetError()); goto end; }
	if(sdlimg->sf->format->BytesPerPixel!=3){ error(ERR_CONT,"Wrong pixelformat \"%s\"",fn); goto end; }

	il->w=sdlimg->sf->w;
	il->h=sdlimg->sf->h;
	il->irat=(float)il->h/(float)il->w;
	imgfit(il->img->pos,il->irat);

	for(i=it;i>=0;i--){
		struct sdlimg *sdlimgscale;
		int size=64;
		if(!replace && il->texs[i].tex && (thumb || i!=TEX_SMALL || !il->texs[i].thumb)) continue;
		if(il->texs[i].loading) continue;
		if(i<TEX_NUM && !(size=load.texsize[i])) continue;
		if((sdlimgscale = sdlimg_gen(SDL_ScaleSurface(sdlimg->sf,sizesel(size,il->w),sizesel(size,il->h))))){
			sdlimg_unref(sdlimg);
			sdlimg=sdlimgscale;
		}
		sdlimg_ref(sdlimg);
		il->texs[i].thumb=thumb;
		debug(DBG_DBG,"ld Loading to tex %s",imgtex_str[i]);
		ldtexload_put(il->texs+i,sdlimg);
		ld=1;
	}
	sdlimg_unref(sdlimg);
	return ld;
end:
	sdlimg_unref(sdlimg);
	il->loadfail=1;
	return ld;
}

void ldffree(struct imgld *il,enum imgtex it){
	if(il->texs[it].loading) return;
	debug(DBG_STA,"ld freeing img tex %s %s",imgtex_str[it],il->fn);
	ldtexload_put(il->texs+it,NULL);
}

/***************************** load check *************************************/

char ldicheck(struct imgld *il,enum imgtex tload,enum imgtex tfree){
	int i;
	char ld=0;
	if(tload>=0 && !il->texs[tload].tex) ld=ldfload(il,tload,0);
	for(i=tfree+1;i<TEX_NUM;i++) if(il->texs[i].tex) ldffree(il,i);
	return ld;
}

char ldfcheck(int imgi,enum imgtex tload,enum imgtex tfree){
	struct img *img=imgget(imgi);
	return img ? ldicheck(img->ld,tload,tfree) : 0;
}

char ldcheck(){
	int i;
	int imgi = dplgetimgi();
	if(ldicheck(defimg.ld,TEX_BIG,TEX_BIG)) return 1;
	if(ldfcheck(imgi+0,dplgetzoom()>0?TEX_FULL:TEX_BIG,TEX_FULL)) return 1;
	if(ldfcheck(imgi+1,TEX_BIG,  TEX_BIG)) return 1;
	if(ldfcheck(imgi+2,TEX_SMALL,TEX_BIG)) return 1;
	if(ldfcheck(imgi-1,TEX_SMALL,TEX_BIG)) return 1;
	if(ldfcheck(imgi+3,TEX_SMALL,TEX_BIG)) return 1;
	if(ldfcheck(imgi+4,TEX_SMALL,TEX_BIG)) return 1;
	if(ldfcheck(imgi-2,TEX_SMALL,TEX_BIG)) return 1;
	for(i= 5;i<=15;i++) if(ldfcheck(imgi+i,TEX_SMALL,TEX_SMALL)) return 1;
	for(i= 3;i<=10;i++) if(ldfcheck(imgi-i,TEX_SMALL,TEX_SMALL)) return 1;
	for(i=16;i<=20;i++) if(ldfcheck(imgi+i,TEX_TINY, TEX_SMALL)) return 1;
	for(i=11;i<=15;i++) if(ldfcheck(imgi-i,TEX_TINY, TEX_SMALL)) return 1;
	for(i=21;i<=30;i++) if(ldfcheck(imgi+i,TEX_TINY, TEX_TINY )) return 1;
	for(i=16;i<=30;i++) if(ldfcheck(imgi-i,TEX_TINY, TEX_TINY )) return 1;
	for(i=31;i<=40;i++) if(ldfcheck(imgi+i,TEX_NONE, TEX_TINY )) return 1;
	for(i=31;i<=40;i++) if(ldfcheck(imgi-i,TEX_NONE, TEX_TINY )) return 1;
	for(i=41;i<=nimg;i++) if(ldfcheck(imgi+i,TEX_NONE,TEX_NONE)) return 1;
	for(i=41;i<=nimg;i++) if(ldfcheck(imgi-i,TEX_NONE,TEX_NONE)) return 1;
	return 0;
}

/***************************** load thread ************************************/

void *ldthread(void *arg){
	while(!sdl.quit) if(!load.enable || !ldcheck()) SDL_Delay(100);
	sdl.quit|=THR_LD;
	return NULL;
}

/***************************** load files init ********************************/

void ldaddfile(char *fn){
	struct img *img=imgadd();
	int i;
	FILE *fd;
	strncpy(img->ld->fn,fn,1024);
	for(i=strlen(fn)-1;i>=0;i--) if(fn[i]=='/'){
		fn[i]='\0';
		snprintf(img->ld->tfn,1024,"%s/thumb/%s",fn,fn+i+1);
		fn[i]='/';
		if((fd=fopen(img->ld->tfn,"rb"))){
			fclose(fd);
			break;
		}
	}
	if(i<0) img->ld->tfn[0]='\0';
}

void ldaddflst(char *flst){
	FILE *fd=fopen(flst,"r");
	char buf[1024];
	if(!fd){ error(ERR_CONT,"ld read flst failed \"%s\"",flst); return; }
	while(!feof(fd)){
		int len;
		if(!fgets(buf,1024,fd)) continue;
		len=strlen(buf);
		while(buf[len-1]=='\n' || buf[len-1]=='\r') buf[--len]='\0';
		ldaddfile(buf);
	}
	fclose(fd);
}

void ldgetfiles(int argc,char **argv){
	imginit(&defimg);
	strncpy(defimg.ld->fn,"data/defimg.png",1024);
	for(;argc;argc--,argv++){
		if(!strcmp(".flst",argv[0]+strlen(argv[0])-5)) ldaddflst(argv[0]);
		else ldaddfile(argv[0]);
	}
}
