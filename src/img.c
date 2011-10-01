#include <stdlib.h>
#include <SDL.h>
#include "img.h"
#include "main.h"
#include "load.h"
#include "exif.h"
#include "cfg.h"
#include "file.h"
#include "pano.h"
#include "eff.h"
#include "gl.h"
#include "act.h"
#include "dpl.h"
#include "prg.h"
#include "help.h"

struct img *defimg;
struct img *dirimg;
struct img *delimg;

enum ilsort {ILS_NONE, ILS_DATE, ILS_FILE, ILS_RND, ILS_NUM, ILS_NXT};

struct imglist {
	struct img **imgs;
	char fn[FILELEN];
	char dir[FILELEN];
	int nimg;
	int nimgo;
	unsigned int simg;
	struct imglist *nxt;
	struct imglist *parent;
	int pos;
	Uint32 last_used;
	struct prg *prg;
	enum ilsort sort;
	char sort_chg;
	struct ldft lf;
} *ils=NULL;

struct ilcfg {
	char init;
	enum ilsort sort_maindir;
	enum ilsort sort_subdir;
	int  maxhistory;
	int  maxloadexif;
} ilcfg = {
	.init=0,
	.maxhistory=200,
	.maxloadexif=100,
};
struct imglist *curils[IL_NUM] = {NULL, NULL};

/***************************** img index operations ***************************/

struct imglist *ilget(int il){
	if(il<0 || il>=IL_NUM) return NULL;
	return curils[il];
}

/* thread: all */
int imggetn(int il){
	struct imglist *curil=ilget(il);
	if(!curil) return 0;
	if(curil->prg && dplgetzoom()==0) return prggetn(curil->prg);
	return curil->nimg;
}
struct prg *ilprg(int il){
	struct imglist *curil=ilget(il);
	return curil ? curil->prg : NULL;
}
const char *ildir(){ return curils[0] && curils[0]->dir[0] ? curils[0]->dir : NULL; }
const char *ilfn(struct imglist *il){ return il && il->fn[0] ? il->fn : NULL; }

/* thread: all */
int imginarrorlimits(int il,int i){
	struct imglist *curil=ilget(il);
	if(!curil) return 0;
	if(i==IMGI_START) return -1;
	if(i==IMGI_END)   return imggetn(il);
	return i;
}

/* thread: all */
int imgidiff(int il,int ia,int ib,int *ira,int *irb){
	int diff;
	struct imglist *curil=ilget(il);
	if(!curil) return 0;
	ia=imginarrorlimits(il,ia);
	ib=imginarrorlimits(il,ib);
	if(ira) *ira=ia;
	if(irb) *irb=ib;
	diff=ib-ia;
	if(dplloop()){
		while(diff> curil->nimg/2) diff-=curil->nimg;
		while(diff<-curil->nimg/2) diff+=curil->nimg;
	}
	return diff;
}

/* thread: all */
struct img *imgget(int il,int i){
	struct imglist *curil=ilget(il);
	if(!curil) return NULL;
	if(i<0 || i>=curil->nimg) return NULL;
	return curil->imgs[i];
}

struct img *ilremoveimg(struct imglist *il,int i,char final){
	struct img *img=il->imgs[i];
	if(i>0 && il->imgs[i-1]->nxt==img)
		il->imgs[i-1]->nxt=img->nxt;
	img->nxt=NULL;
	il->nimg--;
	for(;i<il->nimg;i++) il->imgs[i]=il->imgs[i+1];
	if(!final) il->imgs[i]=img;
	else if(!--il->nimgo) il->imgs[0]=NULL;
	return img;
}

void iladdimg(struct imglist *il,struct img *img,const char *prg){
	if((unsigned int)il->nimg==il->simg) il->imgs=realloc(il->imgs,sizeof(struct img *)*(il->simg+=16));
	il->imgs[il->nimg]=img;
	if(il->nimg) il->imgs[il->nimg-1]->nxt=img;
	il->nimgo=++il->nimg;
	if(prg) prgadd(&il->prg,prg,img);
}

/***************************** image init *************************************/

struct img *imginit(){
	struct img *img=malloc(sizeof(struct img));
	img->free=0;
	img->nxt=NULL;
	img->ld=imgldinit(img);
	img->pos=imgposinit();
	img->exif=imgexifinit();
	img->file=imgfileinit();
	img->pano=imgpanoinit();
	return img;
}

void imgfree(struct img *img){
	if(!img) return;
	if(img->free){
		error(ERR_CONT,"critical: double free img 0x%08lx\n",(long)img);
		return;
	}
	img->free=1;
	imgldfree(img->ld);
	imgposfree(img->pos);
	imgexiffree(img->exif);
	imgfilefree(img->file);
	imgpanofree(img->pano);
	free(img);
}

struct img *imgadd(struct imglist *il,const char *prg){
	struct img *img=imginit();
	iladdimg(il,img,prg);
	return img;
}

struct img *imgdel(int il,int i){
	struct imglist *curil=ilget(il);
	if(!curil) return NULL;
	if(i<0 || i>=curil->nimg) return NULL;
	if(imgfiledir(curil->imgs[i]->file)) return NULL;
	return ilremoveimg(curil,i,0);
}

/***************************** img list managment *****************************/

void ilcfginit(){
	if(ilcfg.init) return;
	ilcfg.sort_maindir = cfggetint("il.random") ? ILS_RND : cfggetint("il.datesort") ? ILS_DATE : ILS_NONE;
	ilcfg.sort_subdir  = cfggetint("il.datesortdir") ? ILS_DATE : ILS_FILE;
	ilcfg.maxhistory   = cfggetint("il.maxhistory");
	ilcfg.maxloadexif  = cfggetint("il.maxloadexif");
	ilcfg.init=1;
}

/* thread: dpl */
void ilsetparent(struct imglist *il){
	if(il==curils[0]) return;
	if(curils[0]){
		struct imglist **pa=&(curils[0]->parent);
		int p=ilcfg.maxhistory;
		while(p && pa[0] && pa[0]!=il){ pa=&(pa[0]->parent); p--; }
		if(!p) error(ERR_CONT,"ilsetparent: maxhistory (%i) reached (is there a loop in history?)",ilcfg.maxhistory);
		if(pa[0]==il) pa[0]=il->parent;
	}
	il->parent=curils[0];
}

char ilfiletime(struct imglist *il,enum eldft act){
	return il->fn[0]!='[' && ldfiletime(&il->lf,act,il->fn);
}

/* thread: dpl */
struct imglist *ilnew(const char *fn,const char *dir){
	struct imglist *il=calloc(1,sizeof(struct imglist));
	snprintf(il->fn,FILELEN,fn);
	snprintf(il->dir,FILELEN,dir);
	ilsetparent(il);
	il->pos=IMGI_START;
	il->nxt=ils;
	ils=il;
	ilfiletime(il,FT_UPDATE);
	debug(DBG_STA,"imglist created for dir: %s",il->fn);
	return il;
}

/* thread: dpl */
void ildestroy(struct imglist *il){
	struct imglist **il2=&ils;
	int i;
	if(!il) return;
	if(il->prg) prgdestroy(il->prg);
	while(il2[0] && il2[0]!=il) il2=&il2[0]->nxt;
	if(il2[0]) il2[0]=il->nxt;
	for(i=0;i<il->nimgo;i++) imgfree(il->imgs[i]);
	if(il->imgs) free(il->imgs);
	debug(DBG_STA,"imglist destroyed for dir: %s",il->fn);
	free(il);
}

void ilfree(struct imglist *il){
	struct img *img;
	debug(DBG_STA,"imglist free for dir: %s",il->fn);
	il->last_used=0;
	if(il->imgs) for(img=il->imgs[0];img;img=img->nxt) ldffree(img->ld,TEX_NONE);
}

/* thread: dpl */
char ilfind(const char *fn,struct imglist **ilret,char setparent){
	struct imglist *il;
	for(il=ils;il;il=il->nxt) if(il->last_used!=1 && !strncmp(il->fn,fn,FILELEN)){
		char ret=!ilfiletime(il,FT_CHECKNOW);
		*ilret=il;
		if(ret && setparent) ilsetparent(il);
		return ret;
	}
	return 0;
}

int ilcleanup_cmp(const void *a,const void *b){
	return (int)(*(Uint32*)b) - (int)(*(Uint32*)a);
}

/* thread: act */
void ilcleanup(){
	struct ilsorted {
		Uint32 last_used;
		struct imglist *il;
	} *ilsorted;
	struct imglist *il,*pa;
	size_t nil=0,i,j;
	unsigned int holdfolders=cfggetuint("img.holdfolders");
	int p;
	for(il=ils;il;il=il->nxt) nil++;
	ilsorted=malloc(sizeof(struct ilsorted)*nil);
	for(il=ils,i=0;il;il=il->nxt,i++){
		ilsorted[i].il=il;
		ilsorted[i].last_used=il->last_used;
	}
	for(i=0;i<nil;i++) if(ilsorted[i].last_used){
		for(pa=ilsorted[i].il->parent,p=ilcfg.maxhistory;pa && p;pa=pa->parent,p--)
			if(pa->last_used && ilsorted[i].last_used>pa->last_used)
				for(j=0;j<nil;j++) if(ilsorted[j].il==pa)
					ilsorted[j].last_used=ilsorted[i].last_used;
		if(!p) error(ERR_CONT,"ilcleanup: maxhistory (%i) reached (is there a loop in history?)",ilcfg.maxhistory);
	}
	qsort(ilsorted,nil,sizeof(struct ilsorted),ilcleanup_cmp);
	for(i=0;i<nil;i++) debug(DBG_DBG,"ilcleanup state %2i: %7i %s",(int)i,ilsorted[i].last_used,ilsorted[i].il->fn);
	for(i=1;i<nil && ilsorted[i].last_used;i++){
		if(ilsorted[i].last_used==1) holdfolders=0;
		if(holdfolders && ilsorted[i].last_used!=ilsorted[i-1].last_used) holdfolders--;
		if(!holdfolders) ilfree(ilsorted[i].il);
	}
	free(ilsorted);
}

char ilsetnxt(struct imglist *il){
	int i;
	char chg=0;
	for(i=0;i<il->nimg;i++){
		struct img *nxt = i<il->nimg-1 ? il->imgs[i+1] : NULL;
		if(nxt!=il->imgs[i]->nxt) chg=1;
		il->imgs[i]->nxt=nxt;
	}
	return chg;
}

/* thread: dpl */
int ilswitch(struct imglist *il,int cil){
	if(cil<0 || cil>=IL_NUM) return IMGI_END;
	if(!il && curils[cil] && curils[cil]->parent) il=curils[cil]->parent;
	if(!il) return IMGI_END;
	debug(DBG_STA,"imglist %i switch to dir: %s",cil,il->fn);
	if(curils[cil]) curils[cil]->pos=dplgetimgi(cil);
	curils[cil]=il;
	if(strcmp("[BASE]",il->fn)) il->last_used=SDL_GetTicks();
	actadd(ACT_ILCLEANUP,NULL,NULL);
	return curils[cil]->pos;
}

/* thread: dpl */
char ilsecswitch(enum ilsecswitch type){
	char buf[FILELEN];
	size_t len;
	struct imglist *il;
	switch(type){
	case ILSS_PRGOFF:
	case ILSS_DIROFF:
		if(!curils[1]) return 1;
		if(type==ILSS_PRGOFF) curils[0]=curils[1];
		curils[1]=NULL;
		actadd(ACT_ILCLEANUP,NULL,NULL);
		return 1;
	case ILSS_PRGON:
		if(!curils[0] || !ilprg(0)) return 0;
		len=strlen(curils[0]->fn);
		if(len<8 || strncmp(curils[0]->fn+len-7,".effprg",7)) return 0;
		snprintf(buf,len-=6,curils[0]->fn);
		if(!isdir(buf)){
			while(len && buf[len-1]!='/' && buf[len-1]!='\\') len--;
			if(len) buf[len-1]='\0';
			else strcpy(buf,".");
			if(!isdir(buf)) return 0;
		}
		if(!(il=floaddir(buf,""))) return 0;
		il->parent=NULL;
		curils[1]=curils[0];
		curils[0]=il;
		curils[0]->last_used=SDL_GetTicks();
		curils[1]->last_used=SDL_GetTicks();
		return 1;
	case ILSS_DIRON:
		return curils[0]!=NULL;
	}
	return 0;
}

/* thread: dpl */
char ilreload(int il,const char *cmd){
	struct imglist *curil=ilget(il);
	if(!curil) return 0;
	if(cmd){
		char buf[FILELEN*3];
		snprintf(buf,FILELEN*3,"perl \"%s\" \"%s\" %s",finddatafile("effprged.pl"),curil->fn,cmd);
		debug(DBG_STA,"reload cmd: %s",buf);
		system(buf);
	}
	curil=curils[il]=floaddir(curil->fn,curil->dir);
	if(curil) curil->last_used=SDL_GetTicks();
	actadd(ACT_ILCLEANUP,NULL,NULL);
	return curil!=NULL;
}

/* thread: dpl */
void ilftcheck(){
	int il;
	for(il=0;il<IL_NUM;il++) if(curils[il] && ilfiletime(curils[il],FT_CHECK)){
		debug(DBG_STA,"reload imglist: %s",curils[il]->fn);
		ilfiletime(curils[il],FT_RESET);
		ilreload(il,NULL);
		effrefresh(EFFREF_ALL);
	}
}

void ilunused(struct imglist *il){
	il->last_used=1;
	actadd(ACT_ILCLEANUP,NULL,NULL);
}

char ilmoveimg(struct imglist *dst,struct imglist *src,const char *fn,size_t len){
	int i;
	for(i=0;i<src->nimg;i++){
		const char *ifn=imgfilefn(src->imgs[i]->file);
		struct img *img;
		if(ifn[len]!='\0' || strncmp(ifn,fn,len)) continue;
		img=ilremoveimg(src,i,1);
		iladdimg(dst,img,fn[len]==';'?fn+len+1:NULL);
		return 1;
	}
	return 0;
}

void imgfinalize(){
	while(ils) ildestroy(ils);
	imgfree(defimg);
	imgfree(dirimg);
	imgfree(delimg);
}

/***************************** image list work ********************************/

void imgrandom(struct imglist *il){
	struct img **oimgs=il->imgs;
	int i,j;
	il->imgs=calloc(sizeof(struct img *),il->simg);
	for(i=0;i<il->nimg;i++){
		j=rand()%il->nimg;
		while(il->imgs[j]) j=(j+1)%il->nimg;
		il->imgs[j]=oimgs[i];
	}
	free(oimgs);
	ilsetnxt(il);
}

int imgsort_filecmp(const void *a,const void *b){
	struct img *ia=*(struct img **)a;
	struct img *ib=*(struct img **)b;
	return strcmp(imgfilefn(ia->file),imgfilefn(ib->file));
}

int imgsort_datecmp(const void *a,const void *b){
	struct img *ia=*(struct img **)a;
	struct img *ib=*(struct img **)b;
	int64_t ad=imgexifdate(ia->exif);
	int64_t bd=imgexifdate(ib->exif);
	if(!ad && !bd) return strcmp(imgfilefn(ia->file),imgfilefn(ib->file));
	if(!ad)   return  1;
	if(!bd)   return -1;
	if(ad<bd) return -1;
	if(ad>bd) return  1;
	return imgsort_filecmp(a,b);
}

/* thread: dpl */
char imgsort(struct imglist *il,char date){
	int i;
	if(il->sort==ILS_DATE) for(i=0;i<il->nimg;i++)
		if(il->nimg<ilcfg.maxloadexif) imgexifload(il->imgs[i]->exif,imgfilefn(il->imgs[i]->file));
		else imgexifsetsortil(il->imgs[i]->exif,il);
	qsort(il->imgs,(size_t)il->nimg,sizeof(struct img *),date?imgsort_datecmp:imgsort_filecmp);
	return ilsetnxt(il);
}

/* thread: dpl */
char ilsortdo(struct imglist *curil){
	switch(curil->sort){
	case ILS_DATE: if(!imgsort(curil,1)) return 0; break;
	case ILS_FILE: if(!imgsort(curil,0)) return 0; break;
	case ILS_RND:  imgrandom(curil); break;
	default: return 0;
	}
	effrefresh(EFFREF_ALL);
	return 1;
}

/* thread: dpl */
char ilsort(int il,struct imglist *curil,enum ilschg chg){
	int i;
	if(!curil && !(curil=ilget(il))) return 0;
	if(chg==ILSCHG_NONE && curil->sort!=ILS_DATE) return 1;
	if(chg==ILSCHG_INC){
		for(i=0;i<ILS_NUM-1;i++){
			if(++curil->sort==ILS_NUM) curil->sort=ILS_NONE+1;
			if(ilsortdo(curil)) break;
		}
		if(i==ILS_NUM) return 0;
		curil->sort_chg=1;
		return 1;
	}
	ilcfginit();
	if(chg==ILSCHG_INIT_MAINDIR) curil->sort=ilcfg.sort_maindir;
	if(chg==ILSCHG_INIT_SUBDIR)  curil->sort=ilcfg.sort_subdir;
	return ilsortdo(curil);
}

const char *ilsortget(int il){
	struct imglist *curil;
	if(!(curil=ilget(il)) || !curil->sort_chg) return NULL;
	switch(curil->sort){
	case ILS_RND:  return _("Rnd");
	case ILS_DATE: return _("Date");
	case ILS_FILE: return _("File");
	default:       return _("None");
	}
}

void ilforallimgs(void (*func)(struct img *img,void *arg),void *arg){
	struct imglist *il;
	struct img *img;
	for(il=ils;il;il=il->nxt)
		for(img=il->imgs[0];img;img=img->nxt)
			func(img,arg);
}

void ilprgfrm(struct imglist *il,const char *prgfrm){
	prgadd(&il->prg,prgfrm,NULL);
}

