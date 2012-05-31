#ifndef _IL_H
#define _IL_H

#define CIL_NUM	2
#define CIL(cil)	((struct imglist*)(long)-(cil+1))
#define CIL_NONE	CIL(CIL_NUM)
#define CIL_ALL		CIL(CIL_NUM+1)

#define IMGI_START	INT_MIN
#define IMGI_END	INT_MAX
#define IMGI_MAP	0x40000000
#define IMGI_CAT	0x60000000
#define IMGI_COL	(0x7FFFFFFF-NPRGCOL*3-2)

#include "img.h"
#include "eff.h"

struct imglist;

/* ilcfg */

void ilcfgswloop();

/* cil */
struct imglist *ilget(struct imglist *il);
char cilset(struct imglist *il,int cil,char reload);
int cilgetacti();
void cilsetact(int actil);
enum cilsecswitch { ILSS_PRGON,ILSS_PRGOFF,ILSS_DIRON,ILSS_DIROFF };
char cilsecswitch(enum cilsecswitch type);

/* il init */
struct imglist *ilnew(const char *fn,const char *dir);
void ildestroy(struct imglist *il);
/* il get */
int ilnimgs(struct imglist *il);
struct prg *ilprg(struct imglist *il);
const char *ildir(struct imglist *il);
const char *ilfn(struct imglist *il);
int ilcimgi(struct imglist *il);
struct img *ilcimg(struct imglist *il);
struct img *ilimg(struct imglist *il,int imgi);
char ilfind(const char *fn,struct imglist **ilret);
const char *ilsortget(struct imglist *il);
int ilrelimgi(struct imglist *il,int imgi);
int ilclipimgi(struct imglist *il,int imgi,char strict);
int ildiffimgi(struct imglist *il,int ia,int ib);
/* il work */
char iladdimg(struct imglist *il,struct img *img,const char *prg);
void ilsetcimgi(struct imglist *il,int imgi);
void ilupdcimgi(struct imglist *il);
char ilreload(struct imglist *il,const char *cmd);
void ilunused(struct imglist *il);
char ilmoveimg(struct imglist *dst,struct imglist *src,const char *fn,size_t len);
void ilsortchg(struct imglist *il,char chg);
char ilsortupd(struct imglist *il,struct img *img);
enum eldft {FT_RESET, FT_UPDATE, FT_CHECK, FT_CHECKNOW};
char ilfiletime(struct imglist *il,enum eldft act);
void ilprgfrm(struct imglist *il,const char *prgfrm);
struct img *ildelcimg(struct imglist *il);
void ileffref(struct imglist *il,enum effrefresh effref);
enum effrefresh ilgeteffref(struct imglist *il);
/* ils */
void ilscleanup();
void ilsftcheck();
int ilsforallimgs(int (*func)(struct img *img,int imgi,struct imglist *il,void *arg),void *arg,char cilonly,int brk);
void ilsfinalize();

#endif
