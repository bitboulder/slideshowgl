#ifndef _MARK_H
#define _MARK_H

#include "img.h"

enum mkcreate { MKC_NO, MKC_YES, MKC_ALLWAYS };

void catalogadd(char *fn);

char *markimgget(struct img *img,enum mkcreate create);

void markssave();

#endif
