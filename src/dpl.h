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

enum dplrefresh { DPLREF_NO, DPLREF_STD, DPLREF_FIT };

void dplrefresh(enum dplrefresh val);
int dplgetimgi();
int dplgetzoom();
char dplineff();
char dplshowinfo();
int dplinputnum();
char *dplhelp();
char dplloop();

struct imgpos *imgposinit();
void imgposfree(struct imgpos * ip);
struct iopt *imgposopt(struct imgpos *ip);
struct ipos *imgposcur(struct imgpos *ip);

char imgfit(struct img *img);

void dplkeyput(SDL_keysym key);
void sdldelay(Uint32 *last,Uint32 delay);
void *dplthread(void *arg);

#endif
