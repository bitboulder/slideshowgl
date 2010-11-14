#include <stdlib.h>
#include "img.h"
#include "load.h"
#include "dpl.h"

char *imgtex_str[]={
	"SMALL","BIG","FULL",
};

struct img *imgs = NULL;
struct img defimg;
int nimg = 0;
int simg = 0;

void imginit(struct img *img,char *fn){
	img->ld=imgldinit(fn);
	img->pos=imgposinit();
}

void imgfree(struct img *img){
	imgldfree(img->ld);
	imgposfree(img->pos);
}

void imgadd(char *fn){
	if(nimg==simg) imgs=realloc(imgs,sizeof(struct img)*(simg+=16));
	imginit(imgs+nimg,fn);
	nimg++;
}

void imgfinalize(){
	int i;
	for(i=0;i<nimg;i++) imgfree(imgs+i);
	free(imgs);
}
