#ifndef _EXIF_H
#define _EXIF_H

enum rot {ROT_0, ROT_90, ROT_180, ROT_270};

struct imgexif *imgexifinit();
void imgexiffree(struct imgexif *exif);

enum rot imgexifrot(struct imgexif *exif);
char *imgexifinfo(struct imgexif *exif);

void imgexifload(struct imgexif *exif,char *fn,char replace);

float imgexifrotf(struct imgexif *exif);
void exifrotate(struct imgexif *exif,int r);

#endif
