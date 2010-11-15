#include <SDL.h>
#include "help.h"

Uint32 SDL_GetPixel(SDL_Surface *surface, int x, int y)
{
	    int bpp = surface->format->BytesPerPixel;
		    /* Here p is the address to the pixel we want to retrieve */
		    Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;

			    switch(bpp) {
					    case 1:
							        return *p;
									        break;

											    case 2:
											        return *(Uint16 *)p;
													        break;

															    case 3:
															        if(SDL_BYTEORDER == SDL_BIG_ENDIAN)
																		            return p[0] << 16 | p[1] << 8 | p[2];
																	        else
																				            return p[0] | p[1] << 8 | p[2] << 16;
																			        break;

																					    case 4:
																					        return *(Uint32 *)p;
																							        break;

																									    default:
																									        return 0;       /* shouldn't happen, but avoids warnings */
																											    }
}

void SDL_PutPixel(SDL_Surface *surface, int x, int y, Uint32 pixel)
{
	    int bpp = surface->format->BytesPerPixel;
		    /* Here p is the address to the pixel we want to set */
		    Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;

			    switch(bpp) {
					    case 1:
							        *p = pixel;
									        break;

											    case 2:
											        *(Uint16 *)p = pixel;
													        break;

															    case 3:
															        if(SDL_BYTEORDER == SDL_BIG_ENDIAN) {
																		            p[0] = (pixel >> 16) & 0xff;
																					            p[1] = (pixel >> 8) & 0xff;
																								            p[2] = pixel & 0xff;
																											        } else {
																														            p[0] = pixel & 0xff;
																																	            p[1] = (pixel >> 8) & 0xff;
																																				            p[2] = (pixel >> 16) & 0xff;
																																							        }
																	        break;

																			    case 4:
																			        *(Uint32 *)p = pixel;
																					        break;
																							    }
}

SDL_Surface *SDL_ScaleSurface(SDL_Surface *Surface, Uint16 Width, Uint16 Height)
{
    if(!Surface || !Width || !Height) return 0;
	if(Width==Surface->w && Height==Surface->h) return 0;
	SDL_Surface *_ret = SDL_CreateRGBSurface(
			Surface->flags,
			Width, Height,
			Surface->format->BitsPerPixel,
			Surface->format->Rmask, Surface->format->Gmask, Surface->format->Bmask, Surface->format->Amask
	);

	float _stretch_factor_x = (float)Width/(float)Surface->w;
	float _stretch_factor_y = (float)Height/(float)Surface->h;

	for(Sint32 y = 0; y < Surface->h; y++)
		for(Sint32 x = 0; x < Surface->w; x++)
			for(Sint32 o_y = 0; o_y < _stretch_factor_y; ++o_y)
				for(Sint32 o_x = 0; o_x < _stretch_factor_x; ++o_x)
					SDL_PutPixel(_ret, (Sint32)(_stretch_factor_x * x) + o_x, (Sint32)(_stretch_factor_y * y) + o_y, SDL_GetPixel(Surface, x, y));
	return _ret;
}
