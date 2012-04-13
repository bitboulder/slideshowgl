#ifndef _ACT_H
#define _ACT_H

#include "img.h"
#include "il.h"

enum act { ACT_SAVEMARKS, ACT_ROTATE, ACT_DELETE, ACT_DELORI, ACT_ILCLEANUP, ACT_MAPCLT, ACT_EXIFCACHE, ACT_NUM };

void actadd(enum act act,struct img *img,struct imglist *il);
void actinit();
int actthread(void *arg);
void actforce();

#endif
