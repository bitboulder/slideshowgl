#include <SDL.h>
#include <SDL_thread.h>
#include "sdl.h"
#include "gl.h"
#include "main.h"

SDL_Surface *screen;

struct sdl sdl = {
	.quit      = 0,
	.scr_w     = 0,
	.scr_h     = 0,
};
	
void sdlresize(int w,int h){
	if(!(screen=SDL_SetVideoMode(w,h,16,SDL_OPENGL|SDL_RESIZABLE))) error(1,"video mode init failed");
	sdl.scr_w = w;
	sdl.scr_h = h;
	glreshape();
}

void sdlinit(){
	if(SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO)<0) error(1,"sdl init failed");
	sdlresize(1024,768);
	SDL_WM_SetCaption("Slideshowgl","slideshowgl");
	glinit();
}

void sdlkey(SDL_keysym key){
	switch(key.sym){
	case SDLK_q: sdl.quit=1; break;
	default: break;
	}
}

char sdlgetevent(){
	SDL_Event ev;
	if(!SDL_WaitEvent(&ev)) error(1,"sdl wait event failed");
	switch(ev.type){
	case SDL_VIDEORESIZE: sdlresize(ev.resize.w,ev.resize.h); break;
	case SDL_KEYDOWN: sdlkey(ev.key.keysym); break;
	case SDL_QUIT: return 0;
	}
	return 1;
}

void *sdlthread(void *arg){
	sdlinit();
	while(!sdl.quit){
		if(!sdlgetevent()) break;
		glpaint();
	}
	SDL_Quit();
	return NULL;
}

