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

char sdlgetfullscreenmode(Uint32 flags,int *w,int *h){
	SDL_Rect** modes=SDL_ListModes(SDL_GetVideoInfo()->vfmt,flags);
	if(modes==(SDL_Rect**)-1) error(ERR_CONT,"sdl All fullscreen modes available");
	else if(modes==(SDL_Rect**)0 || !modes[0]) error(ERR_CONT,"sdl No fullscreen modes available");
	else{
		int i;
		*w=*h=0;
		for(i=0;modes[i];i++) if(modes[i]->w>*w){ *w=modes[i]->w; *h=modes[i]->h; }
	}
	return *w && *h;
}
	
void sdlresize(int w,int h){
	Uint32 flags=SDL_OPENGL|SDL_DOUBLEBUF;
	const SDL_VideoInfo *vi;
	sdl.doresize=0;
	if(sdl.fullscreen && sdlgetfullscreenmode(flags|SDL_FULLSCREEN,&w,&h)){
		debug(DBG_STA,"sdl set video mode fullscreen");
		if(!(screen=SDL_SetVideoMode(w,h,16,flags|SDL_FULLSCREEN))) error(ERR_QUIT,"video mode init failed");
	}else{
		if(!w) w=sdl.scr_w;
		if(!h) h=sdl.scr_h;
		debug(DBG_STA,"sdl set video mode %ix%i",sdl.scr_w,sdl.scr_h);
		if(!(screen=SDL_SetVideoMode(w,h,16,flags|SDL_RESIZABLE))) error(ERR_QUIT,"video mode init failed");
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
	if(SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO)<0) error(ERR_QUIT,"sdl init failed");
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
	while(!sdl.quit){
		if(!sdlgetevent()) break;
		if(sdl.doresize) sdlresize(0,0);
		sdlhidecursor();
		
		while(SDL_GetTicks()-paint_last < (dplineff()?4:12)) ldtexload();

		if(!sdl.sync) sdldelay(&paint_last,16);

		glpaint();
	}
	sdl.quit=1;
	for(i=500;(sdl.quit&THR_OTH)!=THR_OTH && i>0;i--) SDL_Delay(10);
	if(!i){
		error(ERR_CONT,"sdl timeout waiting for threads (%i)",sdl.quit);
	}else{
		ldtexload();
		imgfinalize();
		SDL_Quit();
	}
	return NULL;
}

