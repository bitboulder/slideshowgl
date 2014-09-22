#include <stdio.h>
#include <stdlib.h>

#include "avl.h"

char *imgfilefn(struct imgfile *ifl){ return (char*)ifl; }
const char *imgfiledir(struct imgfile *ifl){ return NULL; }
int64_t imgexifdate(struct imgexif *exif){ return 0; }

struct img *imginit(){
	static int id=0;
	struct img *img=calloc(1,sizeof(struct img));
	char *fn=malloc(32);
	snprintf(fn,32,"img %03i",id++);
	img->file=(struct imgfile *)fn;
	return img;
}

void imgfree(struct img *img){
	free(img->file);
	free(img);
}

int main(){
	struct img *f,*l;
	struct avls *avls;
	int i;
	srand(10);
	avls=avlinit(ILS_FILE,&f,&l);
	for(i=0;i<100;i++){
		struct img *img=imginit();
		#ifdef AVLDEBUG
		printf("INS: %s\n",imgfilefn(img->file));
		#endif
		avlins(avls,img);
	}
	for(i=0;i<1000;i++){
		struct img *img=avlimg(avls,rand()%avlnimg(avls));
		#ifdef AVLDEBUG
		printf("DEL: %s\n",imgfilefn(img->file));
		#endif
		avldel(avls,img);
		imgfree(img);
		img=imginit();
		#ifdef AVLDEBUG
		printf("INS: %s\n",imgfilefn(img->file));
		#endif
		avlins(avls,img);
	}
	while(avlnimg(avls)){
		struct img *img=avlimg(avls,0);
		avldel(avls,img);
		imgfree(img);
	}
	avlfree(avls);
}
