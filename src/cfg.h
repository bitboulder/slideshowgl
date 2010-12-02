#ifndef _CFG_H
#define _CFG_H

#ifndef _
	#if HAVE_GETTEXT
		#define _(X)	gettext(X)
	#else
		#define _(X)	(X)
	#endif
#endif

int cfggetint(char *name);
int cfggetenum(char *name);
float cfggetfloat(char *name);
void cfggetcol(char *name,float *col);
char *cfggetstr(char *name);
void cfgparseargs(int argc,char **argv);

#endif
