#ifndef _IMG_H
#define _IMG_H

enum imgtex {
	TEX_NONE  = -1,
	TEX_TINY  =  0,
	TEX_SMALL =  1,
	TEX_BIG   =  2,
	TEX_FULL  =  3,
	TEX_NUM   =  4,
	TEX_PANO  = -2,
};

extern char *imgtex_str[];

struct img {
	struct img *nxt;
	struct imgld *ld;
	struct imgpos *pos;
	struct imgexif *exif;
	struct imgfile *file;
	struct imgpano *pano;
};
extern struct img **imgs;
extern struct img *defimg;
extern struct img *delimg;
extern int nimg;

struct img *imgget(int i);
struct img *imginit();
struct img *imgadd();
void imgfinalize();
void imgrandom();
struct img *imgdel(int i);

#endif
