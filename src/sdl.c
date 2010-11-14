#include <SDL.h>
#include <SDL_thread.h>
#include "sdl.h"
#include "gl.h"
#include "main.h"
#include "load.h"
#include "dpl.h"
#include "cfg.h"

SDL_Surface *screen;

struct sdl sdl = {
	.quit       = 0,
	.fullscreen = 0,
	.scr_w      = 0,
	.scr_h      = 0,
	.doresize   = 0,
	.sync       = 0,
	.writemode  = 0,
};
	
void sdlresize(int w,int h){
	sdl.doresize=0;
	if(w) sdl.scr_w = w;
	if(h) sdl.scr_h = h;
	if(!sdl.fullscreen){
		debug(DBG_STA,"sdl set video mode %ix%i",sdl.scr_w,sdl.scr_h);
		if(!(screen=SDL_SetVideoMode(sdl.scr_w,sdl.scr_h,16,SDL_OPENGL|SDL_RESIZABLE|SDL_DOUBLEBUF))) error(1,"video mode init failed");
	}else{
		debug(DBG_STA,"sdl set video mode fullscreen");
		if(!(screen=SDL_SetVideoMode(0,0,16,SDL_OPENGL|SDL_FULLSCREEN))) error(1,"video mode init failed");
	}
	ldforcefit();
	glreshape();
}

void sdlinit(){
	sdl.sync=cfggetint("sdl.sync");
	sdl.fullscreen=cfggetint("sdl.fullscreen");
	if(SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO)<0) error(1,"sdl init failed");
	SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL,sdl.sync);
	sdlresize(1024,640);
	SDL_WM_SetCaption("Slideshowgl","slideshowgl");
	SDL_GL_GetAttribute(SDL_GL_SWAP_CONTROL,&sdl.sync);
	if(sdl.sync!=1) sdl.sync=0;
	glinit();
}

void sdlkey(SDL_keysym key){
	dplkeyput(key);
}

char sdlgetevent(){
	SDL_Event ev;
	if(!SDL_PollEvent(&ev)) return 1;
	switch(ev.type){
	case SDL_VIDEORESIZE: sdlresize(ev.resize.w,ev.resize.h); break;
	case SDL_KEYDOWN: sdlkey(ev.key.keysym); break;
	case SDL_QUIT: return 0;
	}
	return 1;
}

void sdldelay(Uint32 *last,Uint32 delay){
	*last=SDL_GetTicks()-*last;
	if(*last<delay) SDL_Delay(delay-*last);
	*last=SDL_GetTicks();
}

void *sdlthread(void *arg){
	Uint32 paint_last=0;
	int i;
	sdlinit();
	while(!sdl.quit){
		if(!sdlgetevent()) break;
		if(sdl.doresize) sdlresize(0,0);
		
		ldtexload();

		if(!sdl.sync) sdldelay(&paint_last,16);

		glpaint();
	}
	for(i=500;(sdl.quit&THR_OTH)!=THR_OTH && i>0;i--) SDL_Delay(10);
	if(!i){
		error(0,"sdl timeout waiting for threads (%i)",sdl.quit);
	}else{
		ldtexload();
		imgfinalize();
		SDL_Quit();
	}
	return NULL;
}

