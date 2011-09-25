#ifndef _MAPLD_H
#define _MAPLD_H

char mapld_check(const char *maptype,int iz,int ix,int iy,char web,char *fn);
void mapldinit();
int mapldthread(void *arg);

#endif
