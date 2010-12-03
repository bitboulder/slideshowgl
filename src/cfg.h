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

int cfggetint(char *name);
int cfggetenum(char *name);
float cfggetfloat(char *name);
void cfggetcol(char *name,float *col);
char *cfggetstr(char *name);
void cfgparseargs(int argc,char **argv);

#endif
