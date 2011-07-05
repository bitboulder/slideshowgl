#ifndef _MAP_H
#define _MAP_H

#include "img.h"
#include "dpl.h"

char mapon();
void mapsdlsize(int *w,int *h);
void mapswtype();
void mapinfo(int i);
char mapgetctl(int i,struct imglist **il,const char **fn,const char **dir);
struct imglist *mapsetpos(int imgi);
void mapinit();
char mapldcheck();
char maprender(char sel);
char mapmove(enum dplev ev,float sx,float sy);

#endif
