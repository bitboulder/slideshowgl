#include <stdlib.h>
#include <SDL.h>
#include "img.h"
#include "main.h"
#include "load.h"
#include "exif.h"
#include "cfg.h"
#include "file.h"
#include "pano.h"
#include "eff.h"
#include "gl.h"
#include "act.h"
#include "dpl.h"
#include "prg.h"

struct img *defimg;
struct img *dirimg;
struct img *delimg;

struct imglist {
	char dir[FILELEN];
	struct img **imgs;
	int nimg;
	int nimgo;
	unsigned int simg;
	struct imglist *nxt;
	struct imglist *parent;
	int pos;
	Uint32 last_used;
	struct prg *prg;
} *ils=NULL;

struct imglist *curil = NULL;

/***************************** img index operations ***************************/

/* thread: all */
int imggetn(){
	if(!curil) return 0;
	if(curil->prg && dplgetzoom()==0) return prggetn(curil->prg);
	return curil->nimg;
}
struct prg *ilprg(){ return curil ? curil->prg : NULL; }

/* thread: all */
int imginarrorlimits(int i){
	if(!curil) return 0;
	if(i==IMGI_START) return -1;
	if(i==IMGI_END)   return imggetn();
	return i;
}

/* thread: all */
int imgidiff(int ia,int ib,int *ira,int *irb){
	int diff;
	if(!curil) return 0;
	ia=imginarrorlimits(ia);
	ib=imginarrorlimits(ib);
	if(ira) *ira=ia;
	if(irb) *irb=ib;
	diff=ib-ia;
	if(dplloop()){
		while(diff> curil->nimg/2) diff-=curil->nimg;
		while(diff<-curil->nimg/2) diff+=curil->nimg;
	}
	return diff;
}

/* thread: all */
struct img *imgget(int i){
	if(!curil) return NULL;
	if(i<0 || i>=curil->nimg) return NULL;
	return curil->imgs[i];
}

/***************************** image init *************************************/

struct img *imginit(){
	struct img *img=malloc(sizeof(struct img));
	img->nxt=NULL;
	img->ld=imgldinit(img);
	img->pos=imgposinit();
	img->exif=imgexifinit();
	img->file=imgfileinit();
	img->pano=imgpanoinit();
	return img;
}

void imgfree(struct img *img){
	if(!img) return;
	imgldfree(img->ld);
	imgposfree(img->pos);
	imgexiffree(img->exif);
	imgfilefree(img->file);
	imgpanofree(img->pano);
	free(img);
}

struct img *imgadd(struct imglist *il,const char *prg){
	struct img *img;
	if((unsigned int)il->nimg==il->simg) il->imgs=realloc(il->imgs,sizeof(struct img *)*(il->simg+=16));
	img=il->imgs[il->nimg]=imginit();
	if(il->nimg) il->imgs[il->nimg-1]->nxt=img;
	il->nimgo=++il->nimg;
	if(prg) prgadd(&il->prg,prg,img);
	return img;
}

struct img *imgdel(int i){
	struct img *img;
	if(!curil) return NULL;
	if(i<0 || i>=curil->nimg) return NULL;
	img=curil->imgs[i];
	if(imgfiledir(img->file)) return NULL;
	if(i>0) curil->imgs[i-1]->nxt=curil->imgs[i]->nxt;
	curil->nimg--;
	for(;i<curil->nimg;i++) curil->imgs[i]=curil->imgs[i+1];
	curil->imgs[i]=img;
	return img;
}

/***************************** img list managment *****************************/

/* thread: dpl */
struct imglist *ilnew(const char *dir){
	struct imglist *il=calloc(1,sizeof(struct imglist));
	strncpy(il->dir,dir,FILELEN);
	il->parent=curil;
	il->pos=IMGI_START;
	il->nxt=ils;
	ils=il;
	debug(DBG_STA,"imglist created for dir: %s",il->dir);
	return il;
}

/* thread: dpl */
void ildestroy(struct imglist *il){
	struct imglist **il2=&ils;
	int i;
	if(!il) return;
	if(il->prg) prgdestroy(il->prg);
	while(il2[0] && il2[0]!=il) il2=&il2[0]->nxt;
	if(il2[0]) il2[0]=il->nxt;
	for(i=0;i<il->nimgo;i++) imgfree(il->imgs[i]);
	if(il->imgs) free(il->imgs);
	debug(DBG_STA,"imglist destroyed for dir: %s",il->dir);
	free(il);
}

void ilfree(struct imglist *il){
	struct img *img;
	debug(DBG_STA,"imglist free for dir: %s",il->dir);
	il->last_used=0;
	for(img=il->imgs[0];img;img=img->nxt) ldffree(img->ld,TEX_NONE);
}

/* thread: dpl */
struct imglist *ilfind(const char *dir){
	struct imglist *il;
	for(il=ils;il;il=il->nxt) if(!strncmp(il->dir,dir,FILELEN)) return il;
	return NULL;
}

int ilcleanup_cmp(const void *a,const void *b){
	return (int)(*(Uint32*)b) - (int)(*(Uint32*)a);
}

/* thread: act */
void ilcleanup(){
	struct ilsort {
		Uint32 last_used;
		struct imglist *il;
	} *ilsort;
	struct imglist *il,*pa;
	size_t nil=0,i,j;
	unsigned int holdfolders=cfggetuint("img.holdfolders");
	for(il=ils;il;il=il->nxt) nil++;
	ilsort=malloc(sizeof(struct ilsort)*nil);
	for(il=ils,i=0;il;il=il->nxt,i++){
		ilsort[i].il=il;
		ilsort[i].last_used=il->last_used;
	}
	for(i=0;i<nil;i++) if(ilsort[i].last_used)
		for(pa=ilsort[i].il->parent;pa;pa=pa->parent)
			if(pa->last_used && ilsort[i].last_used>pa->last_used)
				for(j=0;j<nil;j++) if(ilsort[j].il==pa)
					ilsort[j].last_used=ilsort[i].last_used;
	qsort(ilsort,nil,sizeof(struct ilsort),ilcleanup_cmp);
	for(i=0;i<nil;i++) debug(DBG_DBG,"ilcleanup state %2i: %7i %s\n",(int)i,ilsort[i].last_used,ilsort[i].il->dir);
	for(i=1;i<nil && ilsort[i].last_used;i++){
		if(holdfolders && ilsort[i].last_used!=ilsort[i-1].last_used) holdfolders--;
		if(!holdfolders) ilfree(ilsort[i].il);
	}
	free(ilsort);
}

/* thread: dpl */
int ilswitch(struct imglist *il){
	if(!il && curil && curil->parent) il=curil->parent;
	if(!il) return IMGI_END;
	debug(DBG_STA,"imglist switch to dir: %s",il->dir);
	if(curil) curil->pos=dplgetimgi();
	curil=il;
	if(strcmp("[BASE]",il->dir)) il->last_used=SDL_GetTicks();
	actadd(ACT_ILCLEANUP,NULL);
	return curil->pos;
}

void imgfinalize(){
	while(ils) ildestroy(ils);
	imgfree(defimg);
	imgfree(dirimg);
	imgfree(delimg);
}

/***************************** image list work ********************************/

void imgsetnxt(struct imglist *il){
	int i;
	for(i=0;i<il->nimg;i++)
		il->imgs[i]->nxt = i<il->nimg-1 ? il->imgs[i+1] : NULL;
}

void imgrandom(struct imglist *il){
	struct img **oimgs=il->imgs;
	int i,j;
	il->imgs=calloc(sizeof(struct img *),il->simg);
	for(i=0;i<il->nimg;i++){
		j=rand()%il->nimg;
		while(il->imgs[j]) j=(j+1)%il->nimg;
		il->imgs[j]=oimgs[i];
	}
	free(oimgs);
	imgsetnxt(il);
}

int imgsort_filecmp(const void *a,const void *b){
	struct img *ia=*(struct img **)a;
	struct img *ib=*(struct img **)b;
	return strcmp(imgfilefn(ia->file),imgfilefn(ib->file));
}

int imgsort_datecmp(const void *a,const void *b){
	struct img *ia=*(struct img **)a;
	struct img *ib=*(struct img **)b;
	int64_t ad=imgexifdate(ia->exif);
	int64_t bd=imgexifdate(ib->exif);
	if(!ad && !bd) return strcmp(imgfilefn(ia->file),imgfilefn(ib->file));
	if(!ad)   return  1;
	if(!bd)   return -1;
	if(ad<bd) return -1;
	if(ad>bd) return  1;
	return 0;
}

void imgsort(struct imglist *il,char date){
	int i;
	if(date){
		for(i=0;i<il->nimg;i++){
			// TODO: load imgexif threaded
			glsetbar((float)(i+1)/(float)il->nimg);
			imgexifload(il->imgs[i]->exif,imgfilefn(il->imgs[i]->file));
		}
		glsetbar(0.f);
	}
	qsort(il->imgs,(size_t)il->nimg,sizeof(struct img *),date?imgsort_datecmp:imgsort_filecmp);
	imgsetnxt(il);
}

void ilforallimgs(void (*func)(struct img *img,void *arg),void *arg){
	struct imglist *il;
	struct img *img;
	for(il=ils;il;il=il->nxt)
		for(img=il->imgs[0];img;img=img->nxt)
			func(img,arg);
}

void ilprgfrm(struct imglist *il,const char *prgfrm){
	prgadd(&il->prg,prgfrm,NULL);
}

