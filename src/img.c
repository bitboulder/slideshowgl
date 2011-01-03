#include <stdlib.h>
#include "img.h"
#include "load.h"
#include "exif.h"
#include "cfg.h"
#include "file.h"
#include "pano.h"
#include "eff.h"

struct img **imgs = NULL;
struct img *defimg;
struct img *dirimg;
struct img *delimg;
int nimg = 0;
int nimgo = 0;
unsigned int simg = 0;

/* thread: all */
struct img *imgget(int i){
	if(i<0 || i>=nimg) return NULL;
	return imgs[i];
}

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
	imgldfree(img->ld);
	imgposfree(img->pos);
	imgexiffree(img->exif);
	imgfilefree(img->file);
	imgpanofree(img->pano);
	free(img);
}

struct img *imgadd(){
	if((unsigned int)nimg==simg) imgs=realloc(imgs,sizeof(struct img *)*(simg+=16));
	imgs[nimg]=imginit();
	if(nimg) imgs[nimg-1]->nxt=imgs[nimg];
	nimgo=nimg+1;
	return imgs[nimg++];
}

void imgfinalize(){
	int i;
	for(i=0;i<nimgo;i++) imgfree(imgs[i]);
	free(imgs);
	imgfree(defimg);
	imgfree(dirimg);
}

void imgsetnxt(){
	int i;
	for(i=0;i<nimg;i++) imgs[i]->nxt = i<nimg ? imgs[i+1] : NULL;
}

void imgrandom(){
	struct img **oimgs=imgs;
	int i,j;
	imgs=calloc(sizeof(struct img *),simg);
	for(i=0;i<nimg;i++){
		j=rand()%nimg;
		while(imgs[j]) j=(j+1)%nimg;
		imgs[j]=oimgs[i];
	}
	free(oimgs);
	imgsetnxt();
}

int imgdatesort_cmp(const void *a,const void *b){
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

void imgdatesort(){
	int i;
	for(i=0;i<nimg;i++) imgexifload(imgs[i]->exif,imgfilefn(imgs[i]->file));
	qsort(imgs,(size_t)nimg,sizeof(struct img *),imgdatesort_cmp);
	imgsetnxt();
}

struct img *imgdel(int i){
	struct img *img;
	if(i<0 || i>=nimg) return NULL;
	img=imgs[i];
	if(imgfiledir(img->file)) return NULL;
	if(i>0) imgs[i-1]->nxt=imgs[i]->nxt;
	nimg--;
	for(;i<nimg;i++) imgs[i]=imgs[i+1];
	imgs[i]=img;
	return img;
}
