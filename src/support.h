#ifndef _SUPPORT_H
#define _SUPPORT_H

#define MIN(a,b)	((a)<(b)?(a):(b))

SDL_Surface *SDL_ScaleSurface(SDL_Surface *Surface, Uint16 Width, Uint16 Height);

#endif
