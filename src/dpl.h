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

enum dplev {
	DE_MOVE    = 0x001,
	DE_RIGHT   = 0x002,
	DE_LEFT    = 0x004,
	DE_UP      = 0x008,
	DE_DOWN    = 0x010,
	DE_ZOOMIN  = 0x020,
	DE_ZOOMOUT = 0x040,
	DE_ROT1    = 0x080,
	DE_ROT2    = 0x100,
	DE_PLAY    = 0x200,
	DE_KEY     = 0x400,
	DE_STAT    = 0x800,
};
#define DE_HOR		(DE_RIGHT|DE_LEFT)
#define DE_VER  	(DE_UP|DE_DOWN)
#define DE_ZOOM		(DE_ZOOMIN|DE_ZOOMOUT)
#define DE_DIR(ev)	(((ev)&(DE_RIGHT|DE_UP|DE_ZOOMIN|DE_ROT1))?1:(((ev)&(DE_LEFT|DE_DOWN|DE_ZOOMOUT|DE_ROT2))?-1:0))
#define DE_NEG(ev)	(DE_DIR(ev)>0?(ev<<1):(DE_DIR(ev)<0?(ev>>1):0))

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

void printixy(float sx,float sy);
#define dplevput(e,k)	dplevputx(e,k,0.f,0.f)
void dplevputx(enum dplev ev,SDLKey key,float sx,float sy);
int dplthread(void *arg);

#endif
