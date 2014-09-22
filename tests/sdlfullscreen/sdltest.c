#include <stdlib.h>
#include <stdio.h>
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_thread.h>
#include <GL/glew.h>

void error(char *txt){
	printf("ERROR: %s\n",txt);
	exit(1);
}

int main(){
	int w=1024,h=768;
	Uint32 flags=SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE;
	SDL_Window *wnd;
	SDL_Renderer *rnd;
	if(SDL_CreateWindowAndRenderer(w,h,flags,&wnd,&rnd)<0) error("window creation failed");
	SDL_SetWindowTitle(wnd,"Slideshowgl");
	printf("res %ix%i\n",w,h);
	if(SDL_GL_SetSwapInterval(-1)<0 && SDL_GL_SetSwapInterval(1)<0) error("swap interval failed");
	SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS,"0");
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS,1);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES,4);
	// TODO: set fullscreen if on
	while(1){
		SDL_Event ev;
		if(SDL_PollEvent(&ev)){
			if(ev.type==SDL_KEYDOWN && ev.key.keysym.sym==SDLK_q) break;
			if(ev.type==SDL_KEYDOWN && ev.key.keysym.sym==SDLK_f){
				int wr,hr;
				if(flags&SDL_WINDOW_FULLSCREEN){
					flags&=~SDL_WINDOW_FULLSCREEN;
					SDL_SetWindowFullscreen(wnd,0);
				}else{
					flags|=SDL_WINDOW_FULLSCREEN;
					SDL_SetWindowPosition(wnd,0,0);
					SDL_SetWindowFullscreen(wnd,SDL_WINDOW_FULLSCREEN_DESKTOP);
				}
			}
			if(ev.type==SDL_WINDOWEVENT && ev.window.event==SDL_WINDOWEVENT_RESIZED)
				printf("res %ix%i\n",ev.window.data1,ev.window.data2);
			if(ev.type==SDL_WINDOWEVENT && ev.window.event==SDL_WINDOWEVENT_EXPOSED)
				printf("exp\n");
		}
		glClearColor(0.,0.,0.,1.);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		SDL_GL_SwapWindow(wnd);
	}
}
