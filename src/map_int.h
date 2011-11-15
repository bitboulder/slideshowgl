#ifndef _MAP_INT_H
#define _MAP_INT_H

#include "map.h"

struct mappos {
	double gx,gy;
	int iz;
};

struct mappos *mapgetpos();

#endif
