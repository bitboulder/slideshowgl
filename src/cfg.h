#ifndef _CFG_H
#define _CFG_H

int cfggetint(char *name);
float cfggetfloat(char *name);
char *cfggetstr(char *name);
void cfgsetint(char *name,int i);

#endif
