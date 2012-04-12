#include <stdlib.h>
#include "il.h"
#include "main.h"
#include "img.h"
#include "prg.h"
#include "dpl.h"
#include "load.h"
#include "cfg.h"
#include "act.h"
#include "help.h"
#include "file.h"
#include "img.h"
#include "exif.h"
#include "exich.h"
#include "avl.h"

enum ilsort {ILS_NONE, ILS_DATE, ILS_FILE, ILS_RND, ILS_NUM, ILS_NXT};

struct ilcfg {
	char init;
	enum ilsort sort_maindir;
	enum ilsort sort_subdir;
	int  maxhistory;
	int  maxloadexif;
	char loop;
} ilcfg = {
	.init=0,
	.maxhistory=200,
	.maxloadexif=50,
	.loop=0,
};

struct imglist {
	struct img *imgs,*last;
	char fn[FILELEN];
	char dir[FILELEN];
	int nimg;
	unsigned int simg;
	struct imglist *nxt;
	struct imglist *parent;
	int cimgi;
	Uint32 last_used;
	struct prg *prg;
	enum ilsort sort;
	char sort_chg;
	struct ldft lf;
	struct avls *avls;
} *ils=NULL, *ildel=NULL;

struct curil {
	struct imglist *ils[CIL_NUM];
	int actil;
} curil = {
	.actil=0,
	.ils={NULL,NULL},
};

/******* ilcfg *****************************************************/

void ilcfginit(){
	if(ilcfg.init) return;
	ilcfg.sort_maindir = cfggetint("il.random") ? ILS_RND : cfggetint("il.datesort") ? ILS_DATE : ILS_NONE;
	ilcfg.sort_subdir  = cfggetint("il.datesortdir") ? ILS_DATE : ILS_FILE;
	ilcfg.maxhistory   = cfggetint("il.maxhistory");
	ilcfg.maxloadexif  = cfggetint("il.maxloadexif");
	ilcfg.loop         = cfggetbool("il.loop");
	ilcfg.init=1;
}

void ilcfgswloop(){ ilcfg.loop=!ilcfg.loop; }

/******* cil *******************************************************/

struct imglist *ilget(struct imglist *il){
	long cil=(long)il;
	if(!il) return curil.ils[curil.actil];
	if(cil<0 && cil>=-CIL_NUM) return curil.ils[-cil-1];
	if(il==CIL_NONE) return NULL;
	return il;
}

/* thread: all */
int cilgetacti(){ return curil.actil; }

/* thread: dpl */
void cilsetact(int actil){ if(actil>=0 || actil<CIL_NUM) curil.actil=actil; }

/* thread: dpl */
char cilset(struct imglist *il,int cil){
	if(cil==-1) cil=curil.actil;
	if(cil<0 || cil>=CIL_NUM) return 0;
	if(!il && curil.ils[cil] && curil.ils[cil]->parent) il=curil.ils[cil]->parent;
	if(!il) return 0;
	if(il==curil.ils[cil]) return 1;
	debug(DBG_STA,"imglist %i switch to dir: %s",cil,il->fn);
	curil.ils[cil]=il;
	if(strcmp("[BASE]",il->fn)) il->last_used=SDL_GetTicks();
	actadd(ACT_ILCLEANUP,NULL,NULL);
	return 2;
}

/* thread: dpl */
char cilsecswitch(enum cilsecswitch type){
	char buf[FILELEN];
	size_t len;
	struct imglist *il;
	switch(type){
	case ILSS_PRGOFF:
		if(!curil.ils[1]) return 1;
		curil.ils[0]=curil.ils[1];
	case ILSS_DIROFF:
		if(!curil.ils[1]) return 1;
		curil.ils[1]=NULL;
		actadd(ACT_ILCLEANUP,NULL,NULL);
		return 1;
	case ILSS_PRGON:
		if(!curil.ils[0] || !ilprg(curil.ils[0])) return 0;
		len=strlen(curil.ils[0]->fn);
		if(len<8 || strncmp(curil.ils[0]->fn+len-7,".effprg",7)) return 0;
		snprintf(buf,len-=6,curil.ils[0]->fn);
		if(!(filetype(buf)&FT_DIR)){
			while(len && buf[len-1]!='/' && buf[len-1]!='\\') len--;
			if(len) buf[len-1]='\0';
			else strcpy(buf,".");
			if(!(filetype(buf)&FT_DIR)) return 0;
		}
		if(!(il=floaddir(buf,""))) return 0;
		il->parent=NULL;
		il->cimgi=0;
		curil.ils[1]=curil.ils[0];
		curil.ils[0]=il;
		curil.ils[0]->last_used=SDL_GetTicks();
		curil.ils[1]->last_used=SDL_GetTicks();
		return 1;
	case ILSS_DIRON:
		return curil.ils[0]!=NULL;
	}
	return 0;
}

/******* il sort ***************************************************/

void ilsortinit(struct imglist *il){
	avlcmp cmp;
	struct img *img=il->imgs, *nxt;
	switch(il->sort){
	case ILS_DATE: cmp=avldatecmp; break;
	case ILS_FILE: cmp=avlfilecmp; break;
	case ILS_RND:  cmp=avlrndcmp;  break;
	default:       cmp=NULL;       break;
	}
	il->avls=avlinit(cmp,&il->imgs,&il->last);
	while(img){ nxt=img->nxt; avlins(il->avls,img); img=nxt; }
}

void ilsortfree(struct imglist *il){
	if(il->avls) avlfree(il->avls); 
	il->avls=NULL;
}

void ilsortins(struct imglist *il,struct img *img){
	char *fn=imgfilefn(img->file);
	if(il->sort==ILS_DATE && !exichcheck(img->exif,fn) && il->nimg<ilcfg.maxloadexif) imgexifload(img->exif,fn);
	avlins(il->avls,img);
	il->nimg++;
}

void ilsortdel(struct imglist *il,struct img *img){
	avldel(il->avls,img);
	il->nimg--;
}

char ilsortupd(struct imglist *il,struct img *img){
	if(!(il=ilget(il))) return 0;
	return avlchk(il->avls,img);
}

void ilsortchg(struct imglist *il,char chg){
	if(!(il=ilget(il))) return;
	if(!chg) il->sort=il->dir[0]?ilcfg.sort_subdir:ilcfg.sort_maindir;
	else if(++il->sort==ILS_NUM) il->sort=ILS_NONE+1;
	ilsortfree(il);
	ilsortinit(il);
	if(chg) il->sort_chg=1;
}

/******* il init ***************************************************/

void ilsetparent(struct imglist *il){
	if(il==curil.ils[0]) return;
	if(curil.ils[0]){
		struct imglist **pa=&(curil.ils[0]->parent);
		int p=ilcfg.maxhistory;
		while(p && pa[0] && pa[0]!=il){ pa=&(pa[0]->parent); p--; }
		if(!p) error(ERR_CONT,"ilsetparent: maxhistory (%i) reached (is there a loop in history?)",ilcfg.maxhistory);
		if(pa[0]==il) pa[0]=il->parent;
	}
	il->parent=curil.ils[0];
}

/* thread: dpl */
struct imglist *ilnew(const char *fn,const char *dir){
	struct imglist *il=calloc(1,sizeof(struct imglist));
	ilcfginit();
	snprintf(il->fn,FILELEN,fn);
	snprintf(il->dir,FILELEN,dir);
	ilsetparent(il);
	il->cimgi=IMGI_START;
	il->nxt=ils;
	ils=il;
	ilfiletime(il,FT_UPDATE);
	ilsortchg(il,0);
	debug(DBG_STA,"imglist created for dir: %s",il->fn);
	return il;
}

/* thread: dpl */
void ildestroy(struct imglist *il){
	struct imglist **il2=&ils;
	struct img *img;
	if(!il) return;
	if(il->prg) prgdestroy(il->prg);
	while(il2[0] && il2[0]!=il) il2=&il2[0]->nxt;
	if(il2[0]) il2[0]=il->nxt;
	ilsortfree(il);
	for(img=il->imgs;img;){ struct img *nxt=img->nxt; imgfree(img); img=nxt; }
	debug(DBG_STA,"imglist destroyed for dir: %s",il->fn);
	free(il);
}

void ilfree(struct imglist *il){
	struct img *img;
	debug(DBG_STA,"imglist free for dir: %s",il->fn);
	il->last_used=0;
	if(il->imgs) for(img=il->imgs;img;img=img->nxt) ldffree(img->ld,TEX_NONE);
}

/******* il get ****************************************************/

/* thread: all */
int ilnimgs(struct imglist *il){
	if(!(il=ilget(il))) return 0;
	if(il->prg && dplgetzoom()==0) return prggetn(il->prg);
	return il->nimg;
}
struct prg *ilprg(struct imglist *il){ return (il=ilget(il)) ? il->prg : NULL; }
const char *ildir(struct imglist *il){ return (il=ilget(il)) && il->dir[0] ? il->dir : NULL; }
const char *ilfn(struct imglist *il){ return (il=ilget(il)) && il->fn[0] ? il->fn : NULL; }

/* thread: all */
int ilcimgi(struct imglist *il){
	if(!(il=ilget(il))) return IMGI_START;
	return il->cimgi;
}

/* thread: all */
struct img *ilcimg(struct imglist *il){ return (il=ilget(il)) ? ilimg(il,il->cimgi) : NULL; }

/* thread: all */
struct img *ilimg(struct imglist *il,int imgi){
	struct img *img;
	if(!(il=ilget(il))) return NULL;
	if(imgi<0 || imgi>=il->nimg) return NULL;
	for(img=il->imgs;img && imgi>0;imgi--) img=img->nxt;
	return img;
}

char ilfiletime(struct imglist *il,enum eldft act){
	return il->fn[0]!='[' && ldfiletime(&il->lf,act,il->fn);
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

const char *ilsortget(struct imglist *il){
	if(!(il=ilget(il)) || !il->sort_chg) return NULL;
	switch(il->sort){
	case ILS_RND:  return _("Rnd");
	case ILS_DATE: return _("Date");
	case ILS_FILE: return _("File");
	default:       return _("None");
	}
}

int ilrelimgi(struct imglist *il,int imgi){
	if(!(il=ilget(il))) return 0;
	if(imgi==IMGI_START) return -1;
	if(imgi==IMGI_END) return il->nimg;
	return imgi;
}

/* thread: dpl */
int ilclipimgi(struct imglist *il,int imgi,char strict){
	if(!(il=ilget(il))) return IMGI_START;
	if(ilcfg.loop){
		if(imgi==IMGI_START)  imgi=0;
		if(imgi==IMGI_END)    imgi=il->nimg-1;
		while(imgi<0)         imgi+=il->nimg;
		while(imgi>=il->nimg) imgi-=il->nimg;
	}else if(!strict){
		/* no change for dplmark without loop */
	}else if(dplgetzoom()<0){
		if(imgi<0)         imgi=0;
		if(imgi>=il->nimg) imgi=il->nimg-1;
	}else if(imgi!=IMGI_START && imgi!=IMGI_END){
		if(imgi<0)         imgi=0;
		if(imgi>=il->nimg) imgi=IMGI_END;
	}
	return imgi;
}

int ildiffimgi(struct imglist *il,int ia,int ib){
	int diff;
	if(!(il=ilget(il))) return 0;
	ia=ilrelimgi(il,ia);
	ib=ilrelimgi(il,ib);
	diff=ib-ia;
	if(ilcfg.loop){
		while(diff> il->nimg/2) diff-=il->nimg;
		while(diff<-il->nimg/2) diff+=il->nimg;
	}
	return diff;
}

/******* il work ***************************************************/

char iladdimg(struct imglist *il,struct img *img,const char *prg){
	ilcfginit();
	if(!(il=ilget(il))) return 0;
	ilsortins(il,img);
	if(prg) prgadd(&il->prg,prg,img);
	return 1;
}

/* thread: dpl */
void ilsetcimgi(struct imglist *il,int imgi){
	if((il=ilget(il))) il->cimgi=ilclipimgi(il,imgi,1);
}

void ilupdcimgi(struct imglist *il){
	if((il=ilget(il))) il->cimgi=ilclipimgi(il,il->cimgi,1);
}

/* thread: dpl */
char ilreload(struct imglist *il,const char *cmd){
	struct imglist *iln;
	long cil;
	if(!(il=ilget(il))) return 0;
	if(cmd){
		char buf[FILELEN*3];
		snprintf(buf,FILELEN*3,"perl \"%s\" \"%s\" %s",finddatafile("effprged.pl"),il->fn,cmd);
		debug(DBG_STA,"reload cmd: %s",buf);
		system(buf);
	}
	iln=floaddir(il->fn,il->dir);
	for(cil=0;cil<CIL_NUM;cil++) if(curil.ils[cil]==il) curil.ils[cil]=iln;
	if(iln) iln->last_used=SDL_GetTicks();
	actadd(ACT_ILCLEANUP,NULL,NULL);
	return iln!=NULL;
}

/* thread: dpl */
void ilunused(struct imglist *il){
	il->last_used=1;
	actadd(ACT_ILCLEANUP,NULL,NULL);
}

struct img *ilremoveimg(struct imglist *il,struct img *img,char final){
	if(!(il=ilget(il))) return NULL;
	ilsortdel(il,img);
	ilupdcimgi(il);
	if(!final) iladdimg(ildel,img,NULL);
	return img;
}

/* thread: TODO */
char ilmoveimg(struct imglist *dst,struct imglist *src,const char *fn,size_t len){
	struct img *img;
	for(img=src->imgs;img;img=img->nxt){
		const char *ifn=imgfilefn(img->file);
		if(ifn[len]!='\0' || strncmp(ifn,fn,len)) continue;
		if(!ilremoveimg(src,img,1)) continue;
		iladdimg(dst,img,fn[len]==';'?fn+len+1:NULL);
		return 1;
	}
	return 0;
}

void ilprgfrm(struct imglist *il,const char *prgfrm){
	prgadd(&il->prg,prgfrm,NULL);
}

/* thread: dpl */
struct img *ildelcimg(struct imglist *il){
	struct img *img;
	if(!(il=ilget(il)) || !(img=ilcimg(il)) || imgfiledir(img->file)) return NULL;
	if(!ilremoveimg(il,img,0)) return NULL;
	return img;
}

/******* ils *******************************************************/

int ilscleanup_cmp(const void *a,const void *b){
	return (int)(*(Uint32*)b) - (int)(*(Uint32*)a);
}

/* thread: act */
void ilscleanup(){
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
	qsort(ilsorted,nil,sizeof(struct ilsorted),ilscleanup_cmp);
	for(i=0;i<nil;i++) debug(DBG_DBG,"ilcleanup state %2i: %7i %s",(int)i,ilsorted[i].last_used,ilsorted[i].il->fn);
	for(i=1;i<nil && ilsorted[i].last_used;i++){
		if(ilsorted[i].last_used==1) holdfolders=0;
		if(holdfolders && ilsorted[i].last_used!=ilsorted[i-1].last_used) holdfolders--;
		if(!holdfolders) ilfree(ilsorted[i].il);
	}
	free(ilsorted);
}

/* thread: dpl */
void ilsftcheck(){
	int cil;
	return; /* TODO: enable auto imglist reload */ 
	for(cil=0;cil<CIL_NUM;cil++) if(curil.ils[cil] && ilfiletime(curil.ils[cil],FT_CHECK)){
		debug(DBG_STA,"reload imglist: %s",curil.ils[cil]->fn);
		ilfiletime(curil.ils[cil],FT_RESET);
		ilreload(curil.ils[cil],NULL);
		effrefresh(EFFREF_ALL);
	}
}

int ilsforallimgs(int (*func)(struct img *img,int imgi,struct imglist *il,void *arg),void *arg,char cilonly,int brk){
	struct imglist *il;
	struct img *img;
	int imgi;
	int r=0;
	if(cilonly){
		int cil;
		for(cil=0;cil<CIL_NUM;cil++) if((il=ilget(CIL(cil))))
			for(img=il->imgs,imgi=0;img && imgi<il->nimg*2;img=img->nxt,imgi++){
				r+=func(img,imgi,il,arg);
				if(brk && r>=brk) return 0;
			}
	}else{
		for(il=ils;il;il=il->nxt)
			for(img=il->imgs,imgi=0;img && imgi<il->nimg*2;img=img->nxt,imgi++){
				r+=func(img,imgi,il,arg);
				if(brk && r>=brk) return 0;
			}
	}
	return r;
}

void ilsfinalize(){
	while(ils) ildestroy(ils);
	imgfree(defimg);
	imgfree(dirimg);
	imgfree(delimg);
}

