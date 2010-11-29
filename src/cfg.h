#ifndef _CFG_H
#define _CFG_H

int cfggetint(char *name);
int cfggetenum(char *name);
float cfggetfloat(char *name);
void cfggetcol(char *name,float *col);
char *cfggetstr(char *name);
void cfgparseargs(int argc,char **argv);

#endif
