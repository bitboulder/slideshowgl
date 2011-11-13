#ifndef X_SDL_H
#define X_SDL_H

#include <SDL.h>

extern char sdl_quit;

float sdlrat();
void sdlforceredraw();

char sdlfullscreen(char dst);

void sdlinit();
void sdlquit();
void sdldelay(Uint32 *last,Uint32 delay);
void sdlthreadcheck();
int sdlthread(void *arg);

#endif
