#ifndef _PRG_H
#define _PRG_H

#include "img.h"
#include "il.h"
#include "eff.h"

struct prg;

struct pev {
	float way[2][5];
	float waytime[2];
	int layer;
	char on;
	char off;
};

int prggetn(struct prg *prg);
void prgdestroy(struct prg *prg);
void prgadd(struct prg **prg,const char *txt,struct img *img);
int prgimgidiff(struct imglist *il,int frm,int imgi,struct img *img);
int prgget(struct prg *prg,struct img *img,int frm,int iev,struct pev **pev);
char prgdelay(int frm,float *on,float *stay);
char prgforoneldfrm(struct imglist *il,int frm,char (*func)(struct imgld *il,enum imgtex it),enum imgtex it);

#endif
