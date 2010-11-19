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
	char back;
};

#define ISTAT_TXTSIZE	512
struct istat {
	char txt[ISTAT_TXTSIZE];
	float h;
};

struct icol {
	float g,b,c;
};

enum effrefresh { EFFREF_NO=0x0, EFFREF_IMG=0x1, EFFREF_ALL=0x2, EFFREF_FIT=0x4 };

void effrefresh(enum effrefresh val);
int dplgetimgi();
int dplgetzoom();
char dplineff();
char dplshowinfo();
int dplinputnum();
char *dplhelp();
char dplloop();
struct istat *dplstat();

struct imgpos *imgposinit();
void imgposfree(struct imgpos * ip);
struct iopt *imgposopt(struct imgpos *ip);
struct ipos *imgposcur(struct imgpos *ip);
struct icol *imgposcol(struct imgpos *ip);
void imgpossetmark(struct imgpos *ip);
char imgposmark(struct imgpos *ip);

void dplkeyput(SDL_keysym key);
void *dplthread(void *arg);

#endif
