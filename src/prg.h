#ifndef _PRG_H
#define _PRG_H

#include "img.h"
#include "eff.h"

struct prg;

void prgdestroy(struct prg *prg);
void prgadd(struct prg **prg,const char *txt,struct img *img);
int prgimgidiff(int frm,int imgi);
int prgget(struct prg *prg,struct img *img,int frm,char rev,int iev,struct ipos *way,float *waytime);
char prgforoneldfrm(int frm,char (*func)(struct imgld *il,enum imgtex it),enum imgtex it);

#endif
