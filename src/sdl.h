#ifndef X_SDL_H
#define X_SDL_H

struct sdl {
	char quit;
	int scr_w, scr_h;
	int scrnof_w, scrnof_h;
	char fullscreen;
	char doresize;
	int sync;
	char writemode;
};
extern struct sdl sdl;

void sdlfullscreen();

void *sdlthread(void *arg);

#endif
