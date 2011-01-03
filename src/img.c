#include <stdlib.h>
#include "img.h"
#include "load.h"
#include "exif.h"
#include "cfg.h"
#include "file.h"
#include "pano.h"
#include "eff.h"
#include "main.h"

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
} *ils=NULL;

struct imglist *curil = NULL;

/***************************** img list managment *****************************/

struct imglist *ilnew(const char *dir){
	struct imglist *il=calloc(1,sizeof(struct imglist));
	if(curil) curil->pos=dplgetimgi();
	strncpy(il->dir,dir,FILELEN);
	il->parent=curil;
	il->pos=IMGI_START;
	il->nxt=ils;
	ils=il;
	return il;
}

void ilfree(struct imglist *il){
	/* TODO: imglist cleanup */
}

struct imglist *ilfind(const char *dir){
	struct imglist *il;
	for(il=ils;il;il=il->nxt) if(!strncmp(il->dir,dir,FILELEN)) return il;
	return NULL;
}

int ilswitch(struct imglist *il){
	if(!il && curil && curil->parent) il=curil->parent;
	if(!il) return IMGI_END;
	curil=il;
	return curil->pos;
}

/***************************** img index operations ***************************/

/* thread: all */
int imggetn(){ return curil ? curil->nimg : 0; }

/* thread: all */
int imginarrorlimits(int i){
	if(!curil) return 0;
	if(i==IMGI_START) return -1;
	if(i==IMGI_END)   return curil->nimg;
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

struct img *imgadd(struct imglist *il){
	if((unsigned int)il->nimg==il->simg) il->imgs=realloc(il->imgs,sizeof(struct img *)*(il->simg+=16));
	il->imgs[il->nimg]=imginit();
	if(il->nimg) il->imgs[il->nimg-1]->nxt=il->imgs[il->nimg];
	il->nimgo=il->nimg+1;
	return il->imgs[il->nimg++];
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

/***************************** image list work ********************************/

void imgfinalize(){
	struct imglist *il;
	for(il=ils;il;il=il->nxt){
		int i;
		for(i=0;i<il->nimgo;i++) imgfree(il->imgs[i]);
		free(il->imgs);
	}
	imgfree(defimg);
	imgfree(dirimg);
	imgfree(delimg);
}

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
	if(ad<0 && bd<0) return strcmp(imgfilefn(ia->file),imgfilefn(ib->file));
	if(ad<0)  return  1;
	if(bd<0)  return -1;
	if(ad<bd) return -1;
	if(ad>bd) return  1;
	return 0;
}

void imgsort(struct imglist *il,char date){
	int i;
	for(i=0;i<il->nimg;i++)
		imgexifload(il->imgs[i]->exif,imgfilefn(il->imgs[i]->file));
	qsort(il->imgs,(size_t)il->nimg,sizeof(struct img *),date?imgsort_datecmp:imgsort_filecmp);
	imgsetnxt(il);
}

