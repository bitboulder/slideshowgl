#ifndef _DPL_H
#define _DPL_H

#include <GL/gl.h>
#include <SDL.h>
#include "img.h"

struct ipos {
	float a,s,x,y,m,r;
};

struct iopt {
	char active;
	enum imgtex tex;
	float fitw,fith;
	char back;
};

void dplrefresh();
int dplgetimgi();
int dplgetzoom();
char dplineff();

struct imgpos *imgposinit();
void imgposfree(struct imgpos * ip);
struct iopt *imgposopt(struct imgpos *ip);
struct ipos *imgposcur(struct imgpos *ip);

void imgfit(struct imgpos *ip,float irat);

void dplkeyput(SDL_keysym key);
void sdldelay(Uint32 *last,Uint32 delay);
void *dplthread(void *arg);

#endif
