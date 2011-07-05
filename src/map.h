#ifndef _MAP_H
#define _MAP_H

#include "img.h"
#include "dpl.h"

char mapon();
void mapsdlsize(int *w,int *h);
struct imglist *mapsetpos(int imgi);
void mapinit();
char mapldcheck();
char maprender(char sel);
char mapmove(enum dplev ev,float sx,float sy);

#endif
