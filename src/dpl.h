#ifndef _DPL_H
#define _DPL_H

#include <GL/gl.h>
#include <SDL.h>

struct ipos {
	float a,s,x,y;
};

int dplgetimgi();
int dplgetzoom();

struct imgpos *imgposinit();
void imgposfree(struct imgpos * ip);
char imgposactive(struct imgpos *ip,int p);
GLuint imgpostex(struct imgpos *ip,int p);
struct ipos *imgposcur(struct imgpos *ip,int p);

void dplkeyput(SDL_keysym key);
void sdldelay(Uint32 *last,Uint32 delay);
void *dplthread(void *arg);

#endif
