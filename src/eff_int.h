#ifndef _EFF_INT_H
#define _EFF_INT_H

#include "eff.h"
#include "dpl.h"

struct wh {
	float w,h;
};

struct wh effmaxfit();

void effinit(enum effrefresh effref,enum dplev ev,int imgi);
void effdel(struct imgpos *ip);
void effstaton();
void effpanoend(struct img *img);
char effcatinit(char dst);
int effprgcolinit(float *col,int actimgi);
void effprgcolset(int id);

void effdo();
void effcfginit();

unsigned int effdelay(int imgi,unsigned int dpldur);

#endif
