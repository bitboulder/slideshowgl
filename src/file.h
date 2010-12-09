#ifndef _FILE_H
#define _FILE_H

struct imgfile *imgfileinit();
void imgfilefree(struct imgfile *ifl);
char *imgfilefn(struct imgfile *ifl);
char imgfiletfn(struct imgfile *ifl,char **tfn);

char findfilesubdir(char *dst,char *subdir,char *ext);

void fgetfiles(int argc,char **argv);

#endif
