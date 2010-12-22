#ifndef _CFG_H
#define _CFG_H

#include "config.h"
#if HAVE_GETTEXT
	#include <libintl.h>
#endif

#ifndef _
	#if HAVE_GETTEXT
		#define _(X)	gettext(X)
	#else
		#define _(X)	(X)
	#endif
#endif
#define __(X)	X

int cfggetint(const char *name);
unsigned int cfggetuint(const char *name);
char cfggetbool(const char *name);
int cfggetenum(const char *name);
float cfggetfloat(const char *name);
void cfggetcol(const char *name,float *col);
const char *cfggetstr(const char *name);
void cfgparseargs(int argc,char **argv);

#endif
