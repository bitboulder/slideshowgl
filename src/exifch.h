#ifndef _EXIFCH_H
#define _EXIFCH_H

#include "exif.h"

char exifcachecheck(struct imgexif *exif,const char *fn);
void exifcacheadd(struct imgexif *exif,const char *fn);
void exifcacheload();
void exifcachesave();

#endif
