#ifndef _MAP_INT_H
#define _MAP_INT_H

#include "map.h"

struct mappos {
	double gx,gy;
	int iz;
};

struct maptype {
	char id[8];
	int maxz;
	char url[FILELEN];
};
extern struct maptype *maptypes;
extern unsigned int nmaptypes;

struct mappos *mapgetpos();

#endif
