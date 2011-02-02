#ifndef _MARK_H
#define _MARK_H

#include "img.h"

enum mkcreate { MKC_NO, MKC_YES, MKC_ALLWAYS };

size_t markncat();
char *markcats();
char *markcatfn(int id,const char **na);
void markcatadd(char *fn);

char *markimgget(struct img *img,enum mkcreate create);

void markssave();

#endif
