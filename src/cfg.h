#ifndef _CFG_H
#define _CFG_H

int cfggetint(char *name);
float cfggetfloat(char *name);
char *cfggetstr(char *name);
void cfgparseargs(int argc,char **argv);

#endif
