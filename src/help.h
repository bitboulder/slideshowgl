#ifndef _HELP_H
#define _HELP_H

#include "config.h"

#ifndef MIN
	#define MIN(a,b)	((a)<(b)?(a):(b))
#endif

SDL_Surface *SDL_ScaleSurface(SDL_Surface *Surface, Uint16 Width, Uint16 Height);
void SDL_ColMod(SDL_Surface *Surface, float g, float b, float c);

#if ! HAVE_STRSEP
char *strsep(char **stringp, const char *delim);
#endif

#endif
