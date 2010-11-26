#ifndef X_SDL_H
#define X_SDL_H

#include <SDL.h>

#define ESYNC	E(NONE), E(SDL), E(GLX)
#define E(X) SYNC_##X
enum sdlsync { ESYNC };
#undef E

struct sdlcfg {
	Uint32 hidecursor;
};

struct sdlmove {
	Uint16 base_x, base_y;
	int pos_x, pos_y;
};

struct sdl {
	struct sdlcfg cfg;
	char quit;
	int scr_w, scr_h;
	int scrnof_w, scrnof_h;
	char fullscreen;
	char doresize;
	enum sdlsync sync;
	char writemode;
	Uint32 hidecursor;
	struct sdlmove move;
};
extern struct sdl sdl;

void sdlfullscreen();

void sdlinit();
void sdldelay(Uint32 *last,Uint32 delay);
void sdlthreadcheck();
void *sdlthread(void *arg);

#endif
