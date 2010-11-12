#ifndef X_SDL_H
#define X_SDL_H

struct sdl {
	char quit;
	int scr_w, scr_h;
};
extern struct sdl sdl;

void *sdlthread(void *arg);

#endif
