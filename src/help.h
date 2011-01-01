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

char isdir(const char *fn);

#endif
