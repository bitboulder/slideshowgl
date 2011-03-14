#ifndef _FILE_H
#define _FILE_H

#include "main.h"
#include "img.h"

struct txtimg {
	char txt[FILELEN];
	float col[4];
};

struct imgfile *imgfileinit();
void imgfilefree(struct imgfile *ifl);
char *imgfilefn(struct imgfile *ifl);
char *imgfiledelfn(struct imgfile *ifl);
char imgfiletfn(struct imgfile *ifl,char **tfn);
const char *imgfiledir(struct imgfile *ifl);
struct txtimg *imgfiletxt(struct imgfile *ifl);

char findfilesubdir(char *dst,const char *subdir,const char *ext);

void fgetfiles(int argc,char **argv);

struct imglist *floaddir(const char *fn,const char *dir);

char fimgswitchmod(struct img *img);

#endif
