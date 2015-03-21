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

#define mapg2p(gx,gy,px,py) { \
	(py)=(gy)/180.*M_PI; \
	(py)=asinh(tan(py))/M_PI/2.; \
	(py)=0.5-(py); \
	(px)=(gx)/360.+.5; \
}

#endif
