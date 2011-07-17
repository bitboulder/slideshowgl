#ifndef _LDJPG_H
#define _LDJPG_H

#include <SDL.h>

SDL_Surface *JPG_LoadSwap(const char *fn);
void jpegsave(char *fn,unsigned int w,unsigned int h,unsigned char *buf);

#endif
