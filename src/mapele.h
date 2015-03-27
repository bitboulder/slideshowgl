#ifndef _MAPELE_H
#define _MAPELE_H

#include "map_int.h"

const char *mapelestat(double gsx,double gsy);
char mapeleload(struct mapview *mv);
void mapelerender(struct mapview *mv);
void mapelerenderbar();
void mapeleinit(const char *cachedir);

#endif
