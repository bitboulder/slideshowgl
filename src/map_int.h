#ifndef _MAP_INT_H
#define _MAP_INT_H

#include "map.h"

struct mappos {
	double gx,gy;
	int iz;
};

struct mapview {
	int    iz;
	float  s;
	float  gx,gy;
	double px,py;
	float  pw,ph;
	float  gw,gh;
	double gsx0,gsx1,gsy0,gsy1;
	double psx0,psx1,psy0,psy1;
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
