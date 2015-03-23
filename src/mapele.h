#ifndef _MAPELE_H
#define _MAPELE_H

const char *mapelestat(double gsx,double gsy);
char mapeleload(int gx0,int gx1,int gy0,int gy1);
void mapelerender(double px,double py,double gsx0,double gsx1,double gsy0,double gsy1);
void mapelerenderbar();
void mapeleinit(const char *cachedir);

#endif
