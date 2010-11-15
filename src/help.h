#ifndef _HELP_H
#define _HELP_H

#define MIN(a,b)	((a)<(b)?(a):(b))

SDL_Surface *SDL_ScaleSurface(SDL_Surface *Surface, Uint16 Width, Uint16 Height);

#ifdef __WIN32__
char *strsep(char **stringp, const char *delim);
#endif

#endif
