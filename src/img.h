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
#define IMGI_START	INT_MIN
#define IMGI_END	INT_MAX
#define IMGI_MAP	0x40000000
#define IMGI_CAT	0x60000000
#define IMGI_COL	(0x7FFFFFFF-NPRGCOL*3-2)
#define IMGI_INFO	(IMGI_COL-32)

#define IL_NUM	2

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

enum ilschg { ILSCHG_NONE, ILSCHG_INIT_MAINDIR, ILSCHG_INIT_SUBDIR, ILSCHG_INC };
enum ilsecswitch { ILSS_PRGON,ILSS_PRGOFF,ILSS_DIRON,ILSS_DIROFF };

struct imglist *ilnew(const char *fn,const char *dir);
void ildestroy(struct imglist *il);
char ilfind(const char *fn,struct imglist **ilret,char setparent);
void ilcleanup();
int ilswitch(struct imglist *il,int cil);
char ilsecswitch(enum ilsecswitch type,int *imgi);
char ilreload(int il,const char *cmd);
void ilunused(struct imglist *il);
char ilmoveimg(struct imglist *dst,struct imglist *src,const char *fn,size_t len);
char ilsort(int il,struct imglist *curil,enum ilschg chg);
const char *ilsortget(int il);

struct imglist *ilget(int il);
int imggetn(int il);
struct prg *ilprg(int il);
const char *ildir();
const char *ilfn(struct imglist *il);
int imginarrorlimits(int il,int i);
int imgidiff(int il,int ia,int ib,int *ira,int *irb);

struct img *imgget(int il,int i);
struct img *imginit();
struct img *imgadd(struct imglist *il,const char *prg);
struct img *imgdel(int il,int i);

void imgfinalize();
void ilforallimgs(void (*func)(struct img *img,void *arg),void *arg);
void ilprgfrm(struct imglist *il,const char *prgfrm);

#endif
