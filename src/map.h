#ifndef _MAP_H
#define _MAP_H

#include "img.h"
#include "dpl.h"

char mapon();
void mapsdlsize(int *w,int *h);
void mapswtype();
void mapinfo(int i);
const char *mapgetbasedirs();
char mapgetclt(int i,struct imglist **il,const char **fn,const char **dir);
char mapgetcltpos(int i,float *sx,float *sy);
struct imglist *mapsetpos(int imgi);
void mapimgclt(int izsel);
void mapaddbasedir(const char *dir,const char *name);
void mapinit();
char mapldcheck();
char maprender(char sel);
char mapmove(enum dplev ev,float sx,float sy);
char mapmovepos(float sx,float sy);
char mapmarkpos(float sx,float sy,const char *dir);
char mapsearch(struct dplinput *in);
void mapsavepos();
char maprestorepos();
char mapstatupdate(char *dsttxt);

#endif
