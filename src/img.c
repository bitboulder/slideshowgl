#include <stdlib.h>
#include "img.h"
#include "load.h"
#include "dpl.h"
#include "exif.h"

char *imgtex_str[]={
	"TINY", "SMALL","BIG","FULL",
};

struct img *imgs = NULL;
struct img defimg;
int nimg = 0;
int simg = 0;

struct img *imgget(int i){
	if(i<0 || i>=nimg) return NULL;
	return imgs+i;
}

void imginit(struct img *img){
	img->ld=imgldinit(img);
	img->pos=imgposinit();
	img->exif=imgexifinit();
}

void imgfree(struct img *img){
	imgldfree(img->ld);
	imgposfree(img->pos);
	imgexiffree(img->exif);
}

struct img *imgadd(){
	if(nimg==simg){
		int i;
		imgs=realloc(imgs,sizeof(struct img)*(simg+=16));
		for(i=0;i<nimg;i++) imgldsetimg(imgs[i].ld,imgs+i);
	}
	nimg++;
	imginit(imgs+nimg-1);
	return imgs+nimg-1;
}

void imgfinalize(){
	int i;
	for(i=0;i<nimg;i++) imgfree(imgs+i);
	free(imgs);
}

void imgrandom(){
	struct img *oimgs=imgs;
	int i,j;
	imgs=calloc(sizeof(struct img),simg);
	for(i=0;i<nimg;i++){
		j=rand()%nimg;
		while(imgs[j].ld) j=(j+1)%nimg;
		imgs[j]=oimgs[i];
		imgldsetimg(imgs[j].ld,imgs+j);
	}
	free(oimgs);
}
