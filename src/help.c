#include <SDL.h>
#include <math.h>
#include <iconv.h>
#include "help.h"
#include "main.h"

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


enum truncstr truncstr_tab[256]={0xffffffff};

void truncstr_tabinit(){
	memset(truncstr_tab,0,256*sizeof(enum truncstr));
	truncstr_tab[' ' ]=TS_SPACE;
	truncstr_tab['\t']=TS_SPACE;
	truncstr_tab['\n']=TS_NEWLINE;
	truncstr_tab['\r']=TS_NEWLINE;
}


char *truncstr(char *str,size_t *len,enum truncstr pre,enum truncstr suf){
	size_t l = len && *len ? *len : strlen(str);
	if(truncstr_tab[0]==0xffffffff) truncstr_tabinit();
	if(pre&TS_EQ) pre|=suf;
	if(suf&TS_EQ) suf|=pre;
	while(str[0] && (truncstr_tab[(unsigned char)str[0]]&pre)){ str++; l--; }
	while(l>0 && (truncstr_tab[(unsigned char)str[l-1]]&suf)) l--;
	str[l]='\0';
	if(len) *len=l;
	return str;
}

char isfile(const char *fn){
	FILE *fd=fopen(fn,"r");
	if(!fd) return 0;
	fclose(fd);
	return 1;
}

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

long filesize(const char *fn){
	struct stat st;
	if(stat(fn,&st)) return 0;
	return st.st_size;
}

long filetime(const char *fn){
	struct stat st;
	if(stat(fn,&st)) return 0;
	return st.st_mtime;
}

#else

char isdir(const char *fn){
	if(fileext(fn,".flst")) return 2;
	if(fileext(fn,".effprg")) return 2;
	return 0;
}

long filesize(const char *fn){
	return 1;
}

long filetime(const char *fn){
	return 0;
}

#endif

#ifndef __WIN32__
	#include <sys/stat.h>
	#include <sys/types.h>
#endif
char mkdirm(const char *dir){
#ifdef __WIN32__
	return mkdir(dir)==0;
#else
	return mkdir(dir,00777)==0;
#endif
}

char fileext(const char *fn,size_t len,const char *ext){
	size_t lext=strlen(ext);
	if(!len) len=strlen(fn);
	return len>lext && !strncasecmp(fn+len-lext,ext,lext);
}

#if HAVE_ICONV
	#ifdef __WIN32__
		#define WINCC	(const char **)
	#else
		#define WINCC
	#endif
#endif

/* thread: dpl */
uint32_t unicode2utf8(unsigned short key){
#if HAVE_ICONV
	static iconv_t ic=NULL;
	static char buf[5];
	char *in=(char*)&key;
	char *out=buf;
	size_t nin=sizeof(unsigned short);
	size_t nout=4;
	if(!ic) ic=iconv_open("utf-8","unicode");
	if(ic!=(iconv_t)-1){
		memset(buf,0,5*sizeof(char));
		iconv(ic,NULL,NULL,NULL,NULL);
		if(iconv(ic,WINCC &in,&nin,&out,&nout)!=(size_t)-1 && !nin){
			out=buf;
			return *(uint32_t*)out;
		}
	}
#endif
	return key&0xff;
}

/* thread: all */
void utf8check(char *str){
#if HAVE_ICONV
	static iconv_t *ic=NULL;
	size_t nstr=strlen(str);
	size_t nout=FILELEN;
	char buf[FILELEN];
	char *out=buf;
	int tid=threadid();
	if(!ic) ic=calloc(nthreads(),sizeof(iconv_t));
	if(!ic[tid]) ic[tid]=iconv_open("utf-8","utf-8");
	if(ic[tid]==(iconv_t)-1) return;
	iconv(ic[tid],NULL,NULL,NULL,NULL);
	iconv(ic[tid],WINCC &str,&nstr,&out,&nout);
	if(nstr) *str='\0';
#endif
}

/* thread: gl, eff */
void col_hsl2rgb(float *dst,float *src){
	float c=(1.f-fabsf(2.f*src[2]-1.f))*src[1];
	float h=src[0]*6;
	float h2=h-truncf(h/2.f)*2.f;
	float x=c*(1.f-fabsf(h2-1.f));
	float m=src[2]-.5f*c;
	     if(h<1){ dst[0]=c; dst[1]=x; dst[2]=0; }
	else if(h<2){ dst[0]=x; dst[1]=c; dst[2]=0; }
	else if(h<3){ dst[0]=0; dst[1]=c; dst[2]=x; }
	else if(h<4){ dst[0]=0; dst[1]=x; dst[2]=c; }
	else if(h<5){ dst[0]=x; dst[1]=0; dst[2]=c; }
	else        { dst[0]=c; dst[1]=0; dst[2]=x; }
	dst[0]+=m;
	dst[1]+=m;
	dst[2]+=m;
	dst[3]=src[3];
}

void col_rgb2hsl(float *dst,float *src){
	float M=MAX(src[0],MAX(src[1],src[2]));
	float m=MIN(src[0],MIN(src[1],src[2]));
	float c=M-m;
		 if(c==0)      dst[0]=0.6f;
	else if(M==src[0]) dst[0]=(src[1]-src[2])/c;
	else if(M==src[1]) dst[0]=(src[2]-src[0])/c+2.f;
	else               dst[0]=(src[0]-src[1])/c+4.f;
	dst[0]/=6.f; if(dst[0]<0.f) dst[0]+=1.f;
	dst[2]=(M+m)*0.5f;
	if(c==0) dst[1]=0.f;
	else dst[1]=c/(1.f-fabsf(2*dst[2]-1.f));
	dst[3]=src[3];
}

