#ifndef _IMG_H
#define _IMG_H

enum imgtex {
	TEX_NONE  = -1,
	TEX_SMALL =  0,
	TEX_BIG   =  1,
	TEX_FULL  =  2,
	TEX_NUM   =  3,
};

extern char *imgtex_str[];

struct img {
	struct imgld *ld;
	struct imgpos *pos;
};
extern struct img *imgs;
extern struct img defimg;
extern int nimg;

void imginit(struct img *img,char *fn);
void imgadd(char *fn);
void imgfinalize();

#endif
