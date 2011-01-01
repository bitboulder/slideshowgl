#ifndef _IMG_H
#define _IMG_H

#define IMGTEX	E2(TINY,  0),\
				E2(SMALL, 1),\
				E2(BIG,   2),\
				E2(FULL,  3),\
				E2(NUM,   4),\
				E2(NONE, -1),\
				E2(PANO, -2)

#define E2(X,N)	TEX_##X=N
enum imgtex { IMGTEX };
#undef E2

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
extern struct img *dirimg;
extern struct img *delimg;
extern int nimg;

struct img *imgget(int i);
struct img *imginit();
struct img *imgadd();
void imgfinalize();
void imgrandom();
struct img *imgdel(int i);

#endif
