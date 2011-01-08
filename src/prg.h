#ifndef _PRG_H
#define _PRG_H

#include "img.h"
#include "eff.h"

struct prg;

void prgdestroy(struct prg *prg);
void prgadd(struct prg **prg,const char *txt,struct img *img);
int prgget(struct prg *prg,struct img *img,int frm,char rev,int iev,struct ipos *way,float *waytime);

#endif
