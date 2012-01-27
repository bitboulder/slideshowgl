#ifndef _EXICH_H
#define _EXICH_H

#include "exif.h"

char exichcheck(struct imgexif *exif,const char *fn);
void exichadd(struct imgexif *exif,const char *fn);
void exichload();
void exichsave();

#endif
