#ifndef _HELP_H
#define _HELP_H

#include "config.h"
#include <SDL.h>

#ifndef MIN
	#define MIN(a,b)	((a)<(b)?(a):(b))
#endif
#ifndef MAX
	#define MAX(a,b)	((a)>(b)?(a):(b))
#endif

#define M_PIf		3.14159265358979323846f
#define RAD(x)		((x)/180.f*M_PIf)
#define SIN(x)		sinf(RAD(x))
#define COS(x)		cosf(RAD(x))
#define TAN(x)		tanf(RAD(x))

SDL_Surface *SDL_ScaleSurface(SDL_Surface *Surface, int Width, int Height);
SDL_Surface *SDL_ScaleSurfaceFactor(SDL_Surface *src, int factor, int xoff, int yoff, int fw, int fh, char swap);

#if HAVE_STRSEP
	#include <string.h>
#else
char *strsep(char **stringp, const char *delim);
#endif

#include <strings.h>
#ifndef strcasecmp
int strcasecmp(const char *s1, const char *s2);
#endif
#ifndef strncasecmp
int strncasecmp(const char *s1, const char *s2, size_t n);
#endif

#ifndef popen
FILE *popen(const char *command, const char *type);
int pclose(FILE *stream);
#endif

char isdir(const char *fn);
char isfile(const char *fn);
long filetime(const char *fn);

char fileext(const char *fn,size_t len,const char *ext);

uint32_t unicode2utf8(unsigned short key);
void utf8check(char *str);

#endif
