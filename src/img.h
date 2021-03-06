#ifndef _IMG_H
#define _IMG_H

#include <limits.h>

#define IMGTEX	E2(TINY,  0),\
				E2(SMALL, 1),\
				E2(BIG,   2),\
				E2(FULL,  3),\
				E2(NUM,   4),\
				E2(NONE, -1),\
				E2(PANO, -2)

#define NPRGCOL		128

#define E2(X,N)	TEX_##X=N
enum imgtex { IMGTEX };
#undef E2

struct img {
	double marker; /* TODO: remove */
	char free;
	struct img *nxt, *prv;
	struct imglist *il;
	int frm;
	struct imgld *ld;
	struct imgpos *pos;
	struct imgexif *exif;
	struct imgfile *file;
	struct imgpano *pano;
	struct imghist *hist;
	struct avl *avl;
};
extern struct img *defimg;
extern struct img *dirimg;

struct img *imginit();
void imgfree(struct img *img);
char imgmarker(struct img *img);

#endif
