#ifndef _EFF_H
#define _EFF_H

#include "img.h"
#include <SDL.h>

#define IPOS	E(a) E(s) E(x) E(y) E(r) E(m) E(act) E(back)
#define E(X)	float X; char FILL_##X[sizeof(float)+2*sizeof(Uint32)+sizeof(int)];
struct ecur { IPOS };
#undef E

struct iopt {
	enum imgtex tex;
	char layer;
};

#define ISTAT_TXTSIZE	512
struct istat {
	char txt[ISTAT_TXTSIZE];
	float h;
	char run;
};

struct icol {
	float g,c,b;
};

enum mpcreate { MPC_NO, MPC_YES, MPC_ALLWAYS, MPC_RESET };
enum esw { ESW_CAT, ESW_INFO, ESW_INFOSEL, ESW_HELP, ESW_HIST, ESW_N };

struct imgpos *imgposinit();
void imgposfree(struct imgpos * ip);
struct iopt *imgposopt(struct imgpos *ip);
struct ecur *imgposcur(struct imgpos *ip);
struct icol *imgposcol(struct imgpos *ip);
char *imgposmark(struct img *img,enum mpcreate create);

enum effrefresh {
	EFFREF_NO =0x00,
	EFFREF_IMG=0x01,
	EFFREF_ALL=0x02,
	EFFREF_CLR=0x04,
	EFFREF_FIT=0x08,
	EFFREF_ROT=0x10,
};

void effrefresh(enum effrefresh val);
char effineff();
struct istat *effstat();
float effswf(enum esw id);
float effprgcolf(float **col);
Uint32 efflastchg();


#endif
