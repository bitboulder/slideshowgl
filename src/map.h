#ifndef _MAP_H
#define _MAP_H

#include "img.h"
#include "il.h"
#include "dpl.h"

char mapon();
void mapsdlsize(int *w,int *h);
void mapswtype();
void mapinfo(int i);
const char *mapgetbasedirs();
void mapeditmode();
char mapgetclt(int i,struct imglist **il,const char **fn,const char **dir);
char mapgetcltpos(int i,float *sx,float *sy);
struct imglist *mapsetpos(struct img *img);
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
void mapcltmove(int ci,float sx,float sy);
char mapcltsave(int ci);

#endif
