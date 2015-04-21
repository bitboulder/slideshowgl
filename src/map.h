#ifndef _MAP_H
#define _MAP_H

#include "img.h"
#include "il.h"
#include "dpl.h"

char mapon();
void mapsdlsize(int *w,int *h);
void mapswtype();
void mapinfo(int i);
void mapmousepos(float sx,float sy);
const char *mapgetbasedirs();
char mapdisplaymode();
char mapelevation();
void mapeditmode();
void mapaddurl(char *txt);
char mapgetclt(int i,struct imglist **il,const char **fn,const char **dir);
char mapgetcltpos(int i,float *sx,float *sy);
struct imglist *mapsetpos(struct img *img);
void mapimgclt(int izsel);
char maploadclt();
void mapaddbasedir(const char *dir,const char *name,char cfg);
void mapinit();
char mapldcheck();
char maprender(char sel);
char mapmove(enum dplev ev,float sx,float sy);
char mapmovepos(float sx,float sy);
char mapmarkpos(float sx,float sy,const char *dir);
void mapsearch(struct dplinput *in);
void mapsavepos();
void maprestorepos();
char mapstatupdate(char *dsttxt);
void mapcopypos(float sx,float sy);
struct imglist *mappastepos();
void mapcltmove(int ci,float sx,float sy);
char mapcltsave(int ci);

#endif
