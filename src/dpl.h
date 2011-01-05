#ifndef _DPL_H
#define _DPL_H

#include <SDL.h>
#include "img.h"

struct dplpos {
	int imgi;
	int imgiold;
	int zoom;
	float x,y;
	char writemode;
};

enum dplev {
	DE_JUMPX   = 0x0001,
	DE_JUMPY   = 0x0002,
	DE_MOVE    = 0x0004,
	DE_RIGHT   = 0x0008,
	DE_LEFT    = 0x0010,
	DE_UP      = 0x0020,
	DE_DOWN    = 0x0040,
	DE_ZOOMIN  = 0x0080,
	DE_ZOOMOUT = 0x0100,
	DE_ROT1    = 0x0200,
	DE_ROT2    = 0x0400,
	DE_PLAY    = 0x0800,
	DE_KEY     = 0x1000,
	DE_STAT    = 0x2000,
	DE_SEL     = 0x4000,
	DE_MARK    = 0x8000,
};
#define DE_JUMP		(DE_JUMPX|DE_JUMPY)
#define DE_HOR		(DE_RIGHT|DE_LEFT)
#define DE_VER  	(DE_UP|DE_DOWN)
#define DE_ZOOM		(DE_ZOOMIN|DE_ZOOMOUT)
#define DE_DIR(ev)	(((ev)&(DE_RIGHT|DE_UP|DE_ZOOMIN|DE_ROT1))?1:(((ev)&(DE_LEFT|DE_DOWN|DE_ZOOMOUT|DE_ROT2))?-1:0))
#define DE_NEG(ev)	(DE_DIR(ev)>0?((ev)<<1):(DE_DIR(ev)<0?((ev)>>1):(ev)))

struct dplpos *dplgetpos();
int dplgetimgi();
int dplgetzoom();
char dplshowinfo();
int dplinputnum();
char dplloop();
const char *dplhelp();

void printixy(float sx,float sy);

#define dplevput(e)			dplevputx(e,0,0.f,0.f)
#define dplevputk(k)		dplevputx(DE_KEY,k,0.f,0.f)
#define dplevputp(e,x,y)	dplevputx(e,0,x,y)
void dplevputx(enum dplev ev,SDLKey key,float sx,float sy);
int dplthread(void *arg);

#endif
