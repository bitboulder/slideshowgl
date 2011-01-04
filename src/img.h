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

struct imglist;

struct img {
	struct img *nxt;
	struct imgld *ld;
	struct imgpos *pos;
	struct imgexif *exif;
	struct imgfile *file;
	struct imgpano *pano;
};
extern struct img *defimg;
extern struct img *dirimg;
extern struct img *delimg;

struct imglist *ilnew(const char *dir);
void ildestroy(struct imglist *il);
struct imglist *ilfind(const char *dir);
void ilcleanup();
int ilswitch(struct imglist *il);

int imggetn();
int imginarrorlimits(int i);
int imgidiff(int ia,int ib,int *ira,int *irb);

struct img *imgget(int i);
struct img *imginit();
struct img *imgadd(struct imglist *il);
struct img *imgdel(int i);

void imgfinalize();
void imgrandom(struct imglist *il);
void imgsort(struct imglist *il,char date);

#endif
