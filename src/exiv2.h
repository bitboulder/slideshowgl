#ifndef _EXIV2_H
#define _EXIV2_H

#include "config.h"
#if HAVE_EXIV2

#ifdef __cplusplus
extern "C" {
#endif

struct exiv2data;

struct exiv2data *exiv2init(const char *fn);
void exiv2free(struct exiv2data *edata);
char exiv2getstr(struct exiv2data *edata,const char **key,char *str,int len);
long exiv2getint(struct exiv2data *edata,const char *key);

#ifdef __cplusplus
}
#endif

#endif

#endif
