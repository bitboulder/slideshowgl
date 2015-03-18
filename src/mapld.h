#ifndef _MAPLD_H
#define _MAPLD_H

char mapld_filecheck(const char *fn);
char mapld_check(unsigned int mt,int iz,int ix,int iy,char web,char *fn);
const char *mapldinit();
int mapldthread(void *arg);

#endif
