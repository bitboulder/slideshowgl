#ifndef _EFF_H
#define _EFF_H

#include "dpl.h"

struct wh {
	float w,h;
};

struct imgpos *imgposinit();
void imgposfree(struct imgpos * ip);
struct iopt *imgposopt(struct imgpos *ip);
struct ipos *imgposcur(struct imgpos *ip);
struct icol *imgposcol(struct imgpos *ip);
char *imgposmark(struct imgpos *ip);

enum effrefresh { EFFREF_NO=0x0, EFFREF_IMG=0x1, EFFREF_ALL=0x2, EFFREF_FIT=0x4 };

void effrefresh(enum effrefresh val);
char effineff();
struct wh effmaxfit();
struct istat *effstat();

void effinit(enum effrefresh effref,enum dplev ev,int imgi);
void effdel(struct imgpos *ip);
void effstaton();
void effpanoend(struct img *img);

void effdo();
void effcfginit();

#endif
