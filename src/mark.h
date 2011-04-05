#ifndef _MARK_H
#define _MARK_H

#include "img.h"
#include "dpl.h"

enum mkcreate { MKC_NO, MKC_YES, MKC_ALLWAYS };

size_t markncat();
char *markcats();
char *markcatfn(int id,const char **na);
void markcatadd(char *fn);
void markcatsel(struct dplinput *in);

char *markimgget(struct img *img,enum mkcreate create);
void markchange(size_t id);

void markssave();

#endif
