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
#include "help.h"
#include "exich.h"
#include "hist.h"

struct img *defimg;
struct img *dirimg;
struct img *delimg;

/***************************** image init *************************************/

struct img *imginit(){
	struct img *img=calloc(1,sizeof(struct img));
	img->ld=imgldinit(img);
	img->pos=imgposinit();
	img->exif=imgexifinit();
	img->file=imgfileinit();
	img->pano=imgpanoinit();
	img->hist=imghistinit();
	return img;
}

void imgfree(struct img *img){
	if(!img) return;
	if(img->free){
		error(ERR_CONT,"critical: double free img 0x%08lx\n",(long)img);
		return;
	}
	img->free=1;
	imgldfree(img->ld);
	imgposfree(img->pos);
	imgexiffree(img->exif);
	imgfilefree(img->file);
	imgpanofree(img->pano);
	free(img);
}

