#ifndef _FILE_H
#define _FILE_H

#include "main.h"

struct txtimg {
	char txt[FILELEN];
	float col[4];
};

struct imgfile *imgfileinit();
void imgfilefree(struct imgfile *ifl);
char *imgfilefn(struct imgfile *ifl);
char imgfiletfn(struct imgfile *ifl,char **tfn);
const char *imgfiledir(struct imgfile *ifl);
struct txtimg *imgfiletxt(struct imgfile *ifl);

char findfilesubdir(char *dst,const char *subdir,const char *ext);

void fgetfiles(int argc,char **argv);

int floaddir(struct imgfile *ifl);

#endif
