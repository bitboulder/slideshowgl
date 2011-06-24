#ifndef _DPL_H
#define _DPL_H

#include "img.h"
#include "main.h"

enum dplev {
	DE_STAT    = 0x00000001,
	DE_KEY     = 0x00000002,
	DE_JUMPX   = 0x00000004,
	DE_JUMPY   = 0x00000008,
	DE_MOVE    = 0x00000010,
	DE_RIGHT   = 0x00000020,
	DE_LEFT    = 0x00000040,
	DE_UP      = 0x00000080,
	DE_DOWN    = 0x00000100,
	DE_ROT1    = 0x00000200,
	DE_ROT2    = 0x00000400,
	DE_STOP    = 0x00000800,
	DE_DIR     = 0x00001000,
	DE_MARK    = 0x00002000,
	DE_PLAY    = 0x00004000,
	DE_ZOOMIN  = 0x00008000,
	DE_ZOOMOUT = 0x00010000,
	DE_SEL     = 0x00020000,
	DE_JUMPEND = 0x00040000,
	DE_INIT    = 0x00080000,
	DE_COL     = 0x00100000,
	DE_INFO    = 0x00200000,
};
#define DE_JUMP		(DE_JUMPX|DE_JUMPY)
#define DE_HOR		(DE_RIGHT|DE_LEFT)
#define DE_VER  	(DE_UP|DE_DOWN)
#define DE_ZOOM		(DE_ZOOMIN|DE_ZOOMOUT)
#define DEX_DIR1	(DE_RIGHT|DE_UP|DE_ZOOMIN|DE_ROT1)
#define DEX_DIR2	(DEX_DIR1<<1)
#define DEX_NDIR	(~(unsigned int)(DEX_DIR1|DEX_DIR2))
#define DE_DIR(ev)	(((ev)&DEX_DIR1)?1:(((ev)&DEX_DIR2)?-1:0))
#define DE_NEG(ev)	(((ev)&DEX_NDIR)|(((ev)&DEX_DIR1)<<1)|(((ev)&DEX_DIR2)>>1))

enum dplevsrc { DES_KEY, DES_MOUSE };

struct dplinput {
	unsigned int mode;
	int id;
	char pre[FILELEN];
	char in[FILELEN];
	char post[FILELEN];
};

int dplgetimgi(int il);
int dplgetzoom();
struct dplinput *dplgetinput();
char dplloop();
const char *dplhelp();
int dplgetactil(char *prged);
int dplgetactimgi(int il);
unsigned int dplgetfid();
void dplsetresortil(struct imglist *il);
char *dplgetinfo(unsigned int *sel);

void printixy(float sx,float sy);

#define dplevput(e)				dplevputx(e,0,0.f,0.f,-2,DES_KEY)
#define dplevputk(k)			dplevputx(DE_KEY,k,0.f,0.f,-2,DES_KEY)
#define dplevputp(e,x,y)		dplevputx(e,0,x,y,-2,DES_MOUSE)
#define dplevputi(e,i)			dplevputx(e,0,0.f,0.f,i,DES_MOUSE)
#define dplevputpi(e,x,y,i)		dplevputx(e,0,x,y,i,DES_MOUSE)
#define dplevputs(e,src)		dplevputx(e,0,0.f,0.f,-2,src)
void dplevputx(enum dplev ev,unsigned short key,float sx,float sy,int imgi,enum dplevsrc src);
int dplthread(void *arg);

#endif
