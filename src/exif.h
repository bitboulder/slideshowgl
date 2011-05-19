#ifndef _EXIF_H
#define _EXIF_H

#include <stdint.h>
#include "img.h"

enum rot {ROT_0, ROT_90, ROT_180, ROT_270};

struct imgexif *imgexifinit();
void imgexiffree(struct imgexif *exif);

enum rot imgexifrot(struct imgexif *exif);
int64_t imgexifdate(struct imgexif *exif);
char *imgexifinfo(struct imgexif *exif);
float imgexifrotf(struct imgexif *exif);
void imgexifsetsortil(struct imgexif *exif,struct imglist *sortil);

void imgexifload(struct imgexif *exif,char *fn);

void exifrotate(struct imgexif *exif,int r);

#endif
