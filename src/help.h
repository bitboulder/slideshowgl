#ifndef _HELP_H
#define _HELP_H

#include "config.h"

#ifndef MIN
	#define MIN(a,b)	((a)<(b)?(a):(b))
#endif
#ifndef MAX
	#define MAX(a,b)	((a)>(b)?(a):(b))
#endif

SDL_Surface *SDL_ScaleSurface(SDL_Surface *Surface, Uint16 Width, Uint16 Height);
SDL_Surface *SDL_ScaleSurfaceFactor(SDL_Surface *src, Uint16 factor, Uint16 xoff, Uint16 yoff, Uint16 fw, Uint16 fh, char swap);
void SDL_ColMod(SDL_Surface *Surface, float g, float b, float c);

#if ! HAVE_STRSEP
char *strsep(char **stringp, const char *delim);
#endif

#include <strings.h>
#ifndef strcasecmp
int strcasecmp(const char *s1, const char *s2);
#endif

#endif
