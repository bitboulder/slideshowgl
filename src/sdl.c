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
	.scrnof_w   = 1024,
	.scrnof_h   = 640,
	.doresize   = 0,
	.sync       = 0,
	.writemode  = 0,
	.hidecursor = 0,
};

void sdlfullscreen(){
	if(sdl.fullscreen){
		sdl.scr_w=sdl.scrnof_w;
		sdl.scr_h=sdl.scrnof_h;
		sdl.fullscreen=0;
	}else{
		sdl.scrnof_w=sdl.scr_w;
		sdl.scrnof_h=sdl.scr_h;
		sdl.fullscreen=1;
	}
	sdl.doresize=1;
}
	
void sdlresize(int w,int h){
	const SDL_VideoInfo *vi;
	sdl.doresize=0;
	if(!sdl.fullscreen){
		if(!w) w=sdl.scr_w;
		if(!h) h=sdl.scr_h;
		debug(DBG_STA,"sdl set video mode %ix%i",sdl.scr_w,sdl.scr_h);
		if(!(screen=SDL_SetVideoMode(w,h,16,SDL_OPENGL|SDL_RESIZABLE|SDL_DOUBLEBUF))) error(1,"video mode init failed");
	}else{
		w=1440;
		h=900;
		debug(DBG_STA,"sdl set video mode fullscreen");
		if(!(screen=SDL_SetVideoMode(w,h,16,SDL_OPENGL|SDL_FULLSCREEN))) error(1,"video mode init failed");
	}
	vi=SDL_GetVideoInfo();
	sdl.scr_w=vi->current_w;
	sdl.scr_h=vi->current_h;
	debug(DBG_STA,"sdl get video mode %ix%i",sdl.scr_w,sdl.scr_h);
	glreshape();
	dplrefresh();
}

void sdlinit(){
	sdl.sync=cfggetint("sdl.sync");
	sdl.fullscreen=cfggetint("sdl.fullscreen");
	sdl.cfg.hidecursor=cfggetint("sdl.hidecursor");
	if(SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO)<0) error(1,"sdl init failed");
	SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL,sdl.sync);
	sdlresize(sdl.scrnof_w,sdl.scrnof_h);
	SDL_WM_SetCaption("Slideshowgl","slideshowgl");
	SDL_GL_GetAttribute(SDL_GL_SWAP_CONTROL,&sdl.sync);
	if(sdl.sync!=1) sdl.sync=0;
	glinit();
}

void sdlkey(SDL_keysym key){
	dplkeyput(key);
}

void sdlmotion(){
	SDL_ShowCursor(SDL_ENABLE);
	sdl.hidecursor=SDL_GetTicks()+sdl.cfg.hidecursor;
}

void sdlhidecursor(){
	if(!sdl.hidecursor || SDL_GetTicks()<sdl.hidecursor) return;
	SDL_ShowCursor(SDL_DISABLE);
	sdl.hidecursor=0;
}

char sdlgetevent(){
	SDL_Event ev;
	if(!SDL_PollEvent(&ev)) return 1;
	switch(ev.type){
	case SDL_VIDEORESIZE: sdlresize(ev.resize.w,ev.resize.h); break;
	case SDL_KEYDOWN: sdlkey(ev.key.keysym); break;
	case SDL_MOUSEMOTION: sdlmotion(); break;
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
		sdlhidecursor();
		
		ldtexload();

		if(!sdl.sync) sdldelay(&paint_last,16);

		glpaint();
	}
	sdl.quit=1;
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

