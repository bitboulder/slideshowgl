#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>
#include <SDL_image.h>
#include "sdlimg.h"
#include "main.h"

void sdlimg_unref(struct sdlimg *sdlimg){
	if(!sdlimg || !sdlimg->ref){
		error(ERR_CONT,"sdlimg_unref: unref of %s image",sdlimg?"ref=0":"NULL");
		return;
	}
	if(--sdlimg->ref) return;
	SDL_FreeSurface(sdlimg->sf);
	free(sdlimg);
}

void sdlimg_ref(struct sdlimg *sdlimg){
	if(!sdlimg){
		error(ERR_CONT,"sdlimg_ref: ref of NULL image");
		return;
	}
	sdlimg->ref++;
}

void sdlimg_pixelformat(struct sdlimg *sdlimg){
	SDL_PixelFormat *fmt=sdlimg->sf->format;
	sdlimg->fmt=0;
	if(fmt->BytesPerPixel==3){
		if(fmt->Rmask==0x000000ff && fmt->Gmask==0x0000ff00 && fmt->Bmask==0x00ff0000) sdlimg->fmt=GL_RGB;
		if(fmt->Bmask==0x000000ff && fmt->Gmask==0x0000ff00 && fmt->Rmask==0x00ff0000) sdlimg->fmt=GL_BGR;
	}
	if(fmt->BytesPerPixel==4){
		if(fmt->Rmask==0x000000ff && fmt->Gmask==0x0000ff00 && fmt->Bmask==0x00ff0000 && fmt->Amask==0xff000000) sdlimg->fmt=GL_RGBA;
		if(fmt->Bmask==0x000000ff && fmt->Gmask==0x0000ff00 && fmt->Rmask==0x00ff0000 && fmt->Amask==0xff000000) sdlimg->fmt=GL_BGRA;
	}
}

struct sdlimg* sdlimg_gen(SDL_Surface *sf){
	struct sdlimg *sdlimg;
	if(!sf) return NULL;
	sdlimg=malloc(sizeof(struct sdlimg));
	sdlimg->sf=sf;
	sdlimg->ref=1;
	sdlimg_pixelformat(sdlimg);
	if(!sdlimg->fmt){
		SDL_PixelFormat fmtnew;
		memset(&fmtnew,0,sizeof(SDL_PixelFormat));
		fmtnew.BitsPerPixel=32;
		fmtnew.BytesPerPixel=4;
		fmtnew.Rmask=0x000000ff;
		fmtnew.Gmask=0x0000ff00;
		fmtnew.Bmask=0x00ff0000;
		fmtnew.Amask=0xff000000;
		if((sf=SDL_ConvertSurface(sdlimg->sf,&fmtnew,sdlimg->sf->flags))){
			SDL_FreeSurface(sdlimg->sf);
			sdlimg->sf=sf;
			sdlimg_pixelformat(sdlimg);
		}
	}
	return sdlimg;
}

