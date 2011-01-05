#ifndef _EFF_H
#define _EFF_H

#include "img.h"

struct ipos {
	float a,s,x,y,m,r;
};

struct iopt {
	enum imgtex tex;
	char active;
	char back;
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

struct imgpos *imgposinit();
void imgposfree(struct imgpos * ip);
struct iopt *imgposopt(struct imgpos *ip);
struct ipos *imgposcur(struct imgpos *ip);
struct icol *imgposcol(struct imgpos *ip);
char *imgposmark(struct imgpos *ip);

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


#endif
