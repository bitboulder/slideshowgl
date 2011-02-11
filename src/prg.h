#ifndef _PRG_H
#define _PRG_H

#include "img.h"
#include "eff.h"

struct prg;

struct pev {
	struct ipos way[2];
	float waytime[2];
	int layer;
	char on;
	char off;
};

int prggetn(struct prg *prg);
void prgdestroy(struct prg *prg);
void prgadd(struct prg **prg,const char *txt,struct img *img);
int prgimgidiff(int il,int frm,int imgi);
int prgget(struct prg *prg,struct img *img,int frm,int iev,struct pev **pev);
char prgdelay(int frm,float *on,float *stay);
char prgforoneldfrm(int il,int frm,char (*func)(struct imgld *il,enum imgtex it),enum imgtex it);

#endif
