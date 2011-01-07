#ifndef _PRG_H
#define _PRG_H

#include "img.h"

struct prg;

void prgdestroy(struct prg *prg);
void prgadd(struct prg **prg,const char *txt,struct img *img);

#endif
