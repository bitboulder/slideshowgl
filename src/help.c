#include <SDL.h>
#include <math.h>
#include "help.h"

#define SDL_GetPixel(a,b,c)		SDL_GetPixelSw(a,b,c,0)
Uint32 SDL_GetPixelSw(SDL_Surface *surface, int x, int y, char swap)
{
	int bpp = surface->format->BytesPerPixel;
	/* Here p is the address to the pixel we want to retrieve */
	Uint8 *p = (Uint8 *)surface->pixels + y * (swap?surface->h*surface->format->BytesPerPixel:surface->pitch) + x * bpp;

	switch(bpp) {
	case 1: return *p;
	case 2: return *(Uint16 *)p;
	case 3: if(SDL_BYTEORDER == SDL_BIG_ENDIAN)
				return (Uint32)p[0] << 16 | (Uint32)p[1] << 8 | (Uint32)p[2];
			else
				return (Uint32)p[0] | (Uint32)p[1] << 8 | (Uint32)p[2] << 16;
	case 4: return *(Uint32 *)p;
	default: return 0;
	}
}

void SDL_PutPixel(SDL_Surface *surface, int x, int y, Uint32 pixel)
{
	int bpp = surface->format->BytesPerPixel;
	/* Here p is the address to the pixel we want to set */
	Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;

	switch(bpp) {
	case 1: *p = (Uint8)pixel; break;
	case 2: *(Uint16 *)p = (Uint16)pixel; break;
	case 3: if(SDL_BYTEORDER == SDL_BIG_ENDIAN){
				p[0] = (pixel >> 16) & 0xff;
				p[1] = (pixel >> 8) & 0xff;
				p[2] = pixel & 0xff;
			}else{
				p[0] = pixel & 0xff;
				p[1] = (pixel >> 8) & 0xff;
				p[2] = (pixel >> 16) & 0xff;
			}
		break;
	case 4: *(Uint32 *)p = pixel; break;
	}
}

SDL_Surface *SDL_ScaleSurface(SDL_Surface *Surface, int Width, int Height)
{
	SDL_Surface *_ret;
	float _stretch_factor_x,_stretch_factor_y;
    if(!Surface || !Width || !Height) return 0;
	if(Width==Surface->w && Height==Surface->h) return 0;
	_ret = SDL_CreateRGBSurface(
			Surface->flags,
			Width, Height,
			Surface->format->BitsPerPixel,
			Surface->format->Rmask, Surface->format->Gmask, Surface->format->Bmask, Surface->format->Amask
	);

	_stretch_factor_x = (float)Width/(float)Surface->w;
	_stretch_factor_y = (float)Height/(float)Surface->h;

	for(Sint32 y = 0; y < Surface->h; y++)
		for(Sint32 x = 0; x < Surface->w; x++)
			for(Sint32 o_y = 0; o_y < _stretch_factor_y; ++o_y)
				for(Sint32 o_x = 0; o_x < _stretch_factor_x; ++o_x)
					SDL_PutPixel(_ret, (Sint32)(_stretch_factor_x * (float)x) + o_x, (Sint32)(_stretch_factor_y * (float)y) + o_y, SDL_GetPixel(Surface, x, y));
	return _ret;
}

SDL_Surface *SDL_ScaleSurfaceFactor(SDL_Surface *src, int factor, int xoff, int yoff, int fw, int fh, char swap)
{
	int srcw,srch;
	Sint32 dw,dh,off;
	SDL_Surface *dst;
    if(!src || !factor) return NULL;
	if(swap){ srcw=src->h; srch=src->w; }
	else    { srcw=src->w; srch=src->h; }
	dw = srcw/factor;
	dh = srch/factor;
	if(xoff+fw>dw) fw=dw-xoff;
	if(yoff+fh>dh) fh=dh-yoff;
	if(factor<2 && fw==srcw && fh==srch && !swap) return NULL;
	off = factor/2;
	dst = SDL_CreateRGBSurface(
			src->flags,
			fw, fh,
			src->format->BitsPerPixel,
			src->format->Rmask, src->format->Gmask,
			src->format->Bmask, src->format->Amask
	);

	for(Sint32 y = 0; y < fh; y++)
		for(Sint32 x = 0; x < fw; x++)
			SDL_PutPixel(dst,x,y,SDL_GetPixelSw(src,(x+xoff)*factor+off,(y+yoff)*factor+off,swap));
	return dst;
}

#if ! HAVE_STRSEP
char *strsep(char **stringp, const char *delim){
	int i;
	char *tok;
	if(!stringp || !*stringp) return NULL;
	tok=*stringp;
	for(;**stringp!='\0';(*stringp)++){
		for(i=0;delim[i]!='\0';i++) if(**stringp==delim[i]){
			**stringp='\0';
			(*stringp)++;
			return tok;
		}
	}
	*stringp=NULL;
	return tok;
}
#endif


#if HAVE_STAT
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

char isdir(const char *fn){
	struct stat st;
	if(stat(fn,&st)) return 0;
	if(S_ISDIR(st.st_mode)) return 1;
	if(fileext(fn,0,".flst")) return 2;
	if(fileext(fn,0,".effprg")) return 2;
	return 0;
}

long filetime(const char *fn){
	struct stat st;
	if(stat(fn,&st)) return 0;
	return st.st_mtime;
}

#else

char isdir(const char *fn __attribute__((unused))){
	if(fileext(fn,".flst")) return 2;
	if(fileext(fn,".effprg")) return 2;
	return 0;
}

long filetime(const char *fn){
	return 0;
}

#endif

char fileext(const char *fn,size_t len,const char *ext){
	size_t lext=strlen(ext);
	if(!len) len=strlen(fn);
	return len>lext+1 && !strncasecmp(fn+len-lext,ext,lext);
}

uint32_t unicode2utf8(unsigned short key){
	static iconv_t ic=NULL;
	static char buf[5];
	char *in=(char*)&key;
	char *out=buf;
	size_t nin=sizeof(unsigned short);
	size_t nout=4;
	if(!ic) ic=iconv_open("utf-8","unicode");
	if(ic!=(iconv_t)-1){
		memset(buf,0,5*sizeof(char));
		if(iconv(ic,&in,&nin,&out,&nout)!=(size_t)-1 && !nin){
			out=buf;
			return *(uint32_t*)out;
		}
	}
	return key&0xff;
}
