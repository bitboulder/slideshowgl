#ifndef X_SDL_H
#define X_SDL_H

#include <SDL.h>

struct sdlcfg {
	Uint32 hidecursor;
};

struct sdl {
	struct sdlcfg cfg;
	char quit;
	int scr_w, scr_h;
	int scrnof_w, scrnof_h;
	char fullscreen;
	char doresize;
	int sync;
	char writemode;
	Uint32 hidecursor;
};
extern struct sdl sdl;

void sdlfullscreen();

void *sdlthread(void *arg);

#endif
