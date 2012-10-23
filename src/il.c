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
#include "ilo.h"

struct ilcfg {
	char init;
	enum avlcmp sort_maindir;
	enum avlcmp sort_subdir;
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
	struct ilo *opt[ILO_N];
	char fn[FILELEN];
	char dir[FILELEN];
	unsigned int simg;
	struct imglist *nxt;
	struct imglist *parent;
	int cimgi;
	Uint32 last_used;
	struct prg *prg;
	enum avlcmp sort;
	char sort_chg;
	struct ldft lf;
	struct avls *avls;
	enum effrefresh effref;
	char subdir;
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
	if(il==CIL_NONE || il==CIL_ALL) return NULL;
	if(il==CIL_DEL) return ildel;
	return il;
}

/* thread: all */
int cilgetacti(){ return curil.actil; }

/* thread: dpl */
void cilsetact(int actil){ if(actil>=0 || actil<CIL_NUM) curil.actil=actil; }

void ilsetparent(struct imglist *il,char reload){
	struct imglist *cil=curil.ils[0];
	if(reload && cil) cil=cil->parent;
	if(il==cil) return;
	if(cil){
		struct imglist **pa=&(cil->parent);
		int p=ilcfg.maxhistory;
		while(p && pa[0] && pa[0]!=il){ pa=&(pa[0]->parent); p--; }
		if(!p) error(ERR_CONT,"ilsetparent: maxhistory (%i) reached (is there a loop in history?)",ilcfg.maxhistory);
		if(pa[0]==il) pa[0]=il->parent;
	}
	il->parent=cil;
}

/* thread: dpl */
char cilset(struct imglist *il,int cil,char reload){
	char nopa=0;
	if(cil==-1) cil=curil.actil;
	if(cil<0 || cil>=CIL_NUM) return 0;
	if((nopa=(!il && curil.ils[cil] && curil.ils[cil]->parent))) il=curil.ils[cil]->parent;
	if(!il) return 0;
	if(il==curil.ils[cil]) return 1;
	if(!nopa && !cil) ilsetparent(il,reload);
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
	il->avls=avlinit(il->sort,&il->imgs,&il->last);
}

void ilsortfree(struct imglist *il){
	if(il->avls) avlfree(il->avls); 
	il->avls=NULL;
}

void ilsortins(struct img *img){
	if(img->il){
		char *fn=imgfilefn(img->file);
		if(img->il->sort==ILS_DATE && !exichcheck(img->exif,fn) && avlnimg(img->il->avls)<ilcfg.maxloadexif) imgexifload(img->exif,fn);
		avlins(img->il->avls,img);
	}
}

void ilsortdel(struct img *img){ if(img->il) avldel(img->il->avls,img); }

char ilsortupd(struct img *img){
	if(!img->il) return 0;
	return avlchk(img->il->avls,img);
}

void ilsortchg(struct imglist *il){
	enum avlcmp sortori;
	if(!(il=ilget(il))) return;
	sortori=il->sort;
	do{
		if(++il->sort==ILS_NUM) il->sort=0;
	}while(!avlsortchg(il->avls,il->sort) && il->sort!=sortori);
	il->sort_chg=1;
}

/******* il init ***************************************************/

/* thread: dpl */
struct imglist *ilnew(const char *fn,const char *dir){
	struct imglist *il=calloc(1,sizeof(struct imglist));
	int o;
	ilcfginit();
	snprintf(il->fn,FILELEN,fn);
	snprintf(il->dir,FILELEN,dir);
	il->cimgi=IMGI_START;
	il->nxt=ils;
	ils=il;
	ilfiletime(il,FT_UPDATE);
	il->sort=il->dir[0]?ilcfg.sort_subdir:ilcfg.sort_maindir;
	ilsortinit(il);
	debug(DBG_STA,"imglist created for dir: %s",il->fn);
	for(o=0;o<ILO_N;o++) il->opt[o]=ilonew();
	return il;
}

/* thread: dpl */
void ildestroy(struct imglist *il){
	struct imglist **il2=&ils;
	struct img *img;
	int o;
	if(!il) return;
	for(o=0;o<ILO_N;o++) ilofree(il->opt[o]);
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
	return avlnimg(il->avls);
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
	if(!(il=ilget(il))) return NULL;
	if(!imgi) return il->imgs;
	if(imgi<0 || imgi>=avlnimg(il->avls)) return NULL;
	return avlimg(il->avls,imgi);
}

char ilfiletime(struct imglist *il,enum eldft act){
	return il->fn[0]!='[' && ldfiletime(&il->lf,act,il->fn);
}

/* thread: dpl */
char ilfind(const char *fn,struct imglist **ilret){
	struct imglist *il;
	for(il=ils;il;il=il->nxt) if(il->last_used!=1 && !strncmp(il->fn,fn,FILELEN)){
		char ret=!ilfiletime(il,FT_CHECKNOW);
		*ilret=il;
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
	if(imgi==IMGI_END) return avlnimg(il->avls);
	return imgi;
}

/* thread: dpl */
int ilclipimgi(struct imglist *il,int imgi,char strict){
	int nimg;
	if(!(il=ilget(il))) return IMGI_START;
	nimg=avlnimg(il->avls);
	if(ilcfg.loop){
		if(imgi==IMGI_START) imgi=0;
		if(imgi==IMGI_END)   imgi=nimg-1;
		while(imgi<0)        imgi+=nimg;
		while(imgi>=nimg)    imgi-=nimg;
	}else if(!strict){
		/* no change for dplmark without loop */
	}else if(dplgetzoom()<0){
		if(imgi<0)     imgi=0;
		if(imgi>=nimg) imgi=nimg-1;
	}else if(imgi!=IMGI_START && imgi!=IMGI_END){
		if(imgi<0)     imgi=0;
		if(imgi>=nimg) imgi=IMGI_END;
	}
	return imgi;
}

int ildiffimgi(struct imglist *il,int ia,int ib){
	int diff,nimg;
	if(!(il=ilget(il))) return 0;
	nimg=avlnimg(il->avls);
	ia=ilrelimgi(il,ia);
	ib=ilrelimgi(il,ib);
	diff=ib-ia;
	if(ilcfg.loop){
		while(diff> nimg/2) diff-=nimg;
		while(diff<-nimg/2) diff+=nimg;
	}
	return diff;
}

char ildired(struct imglist *il){
	if(!(il=ilget(il))) return 0;
	return il->subdir && (filetype(il->fn)&FT_DIR);
}

/******* il work ***************************************************/

char iladdimg(struct imglist *il,struct img *img,const char *prg){
	ilcfginit();
	if(!(il=ilget(il))) return 0;
	if(img->il) error(ERR_CONT,"iladdimg: image still in il \"%s\"",imgfilefn(img->file));
	img->il=il;
	ilsortins(img);
	if(prg) prgadd(&il->prg,prg,img);
	if(filetype(imgfilefn(img->file))&FT_DIR) il->subdir=1;
	ilsetopt(img,ILO_EXIF);
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

struct img *ilremoveimg(struct img *img,char final){
	int o;
	if(!img->il) return img;
	for(o=0;o<ILO_N;o++) ilodel(img->il->opt[o],img);
	ilsortdel(img);
	ilupdcimgi(img->il);
	img->il=NULL;
	if(!final){
		if(!ildel) ildel=ilnew("[DEL]","");
		iladdimg(ildel,img,NULL);
	}
	return img;
}

/* thread: TODO */
char ilmoveimg(struct imglist *dst,struct imglist *src,const char *fn,size_t len){
	struct img *img;
	for(img=src->imgs;img;img=img->nxt){
		const char *ifn=imgfilefn(img->file);
		if(ifn[len]!='\0' || strncmp(ifn,fn,len)) continue;
		if(!ilremoveimg(img,1)) continue;
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
	if(!ilremoveimg(img,0)) return NULL;
	return img;
}

void ileffref(struct imglist *il,enum effrefresh effref){
	if(il==CIL_ALL){
		int cil;
		for(cil=0;cil<CIL_NUM;cil++) if((il=ilget(CIL(cil)))) il->effref|=effref;
	}else if((il=ilget(il))) il->effref|=effref;
}

enum effrefresh ilgeteffref(struct imglist *il){
	enum effrefresh effref;
	if(!(il=ilget(il))) return EFFREF_NO;
	effref=il->effref;
	il->effref=EFFREF_NO;
	return effref;
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
	struct imglist *il;
	return; /* TODO: enable auto imglist reload */ 
	for(cil=0;cil<CIL_NUM;cil++) if((il=ilget(CIL(cil))) && ilfiletime(il,FT_CHECK)){
		debug(DBG_STA,"reload imglist: %s",il->fn);
		ilfiletime(il,FT_RESET);
		ilreload(il,NULL);
		ileffref(il,EFFREF_ALL);
	}
}

int ilsforallimgs(int (*func)(struct img *img,void *arg),void *arg,char cilonly,int brk,enum ilopt o){
	struct imglist *il;
	struct img *img;
	int r=0;
	struct iloi *iloi,*iloin;
#if defined ILODEBUG && defined ILODEBUGMISS
	o=ILO_ALL;
#endif
	if(cilonly){
		int cil;
		for(cil=-1;cil<CIL_NUM;cil++) if((il=ilget(cil<0?CIL_DEL:CIL(cil)))){
			if(o==ILO_ALL) for(img=il->imgs;img;img=img->nxt){
				r+=func(img,arg);
				if(brk && r>=brk) return r;
			}else for(iloi=ilofst(il->opt[o]);iloi;iloi=iloin){
				iloin=iloi->nxt;
				r+=func(iloi->ptr,arg);
				if(brk && r>=brk) return r;
			}
		}
	}else{
		for(il=ils;il;il=il->nxt)
			if(o==ILO_ALL) for(img=il->imgs;img;img=img->nxt){
				r+=func(img,arg);
				if(brk && r>=brk) return r;
			}else for(iloi=ilofst(il->opt[o]);iloi;iloi=iloin){
				iloin=iloi->nxt;
				r+=func(iloi->ptr,arg);
				if(brk && r>=brk) return r;
			}
	}
	return r;
}

void ilsfinalize(){
	while(ils) ildestroy(ils);
	imgfree(defimg);
	imgfree(dirimg);
}


#ifndef ILODEBUG
	#define ilodbg(A,B,C)
#else
void ilodbg(struct img *img,enum ilopt opt,const char *txt){
	const char *opts[]={"gl","eff","load","exif","n","all"};
	const char *fn=imgfilefn(img->file);
	if(img->il) printf("%8i - 0x%08lx-%-4s: %9s 0x%08lx %s\n",SDL_GetTicks(),(unsigned long)img->il->opt[opt],opts[opt],txt,(unsigned long)img,fn?fn:"");
}
#endif

void ilsetopt(struct img *img,enum ilopt opt){
	ilodbg(img,opt,"set");
	if(img->il) iloset(img->il->opt[opt],img);
}
void ildelopt(struct img *img,enum ilopt opt){
	ilodbg(img,opt,"del");
	if(img->il) ilodel(img->il->opt[opt],img);
}
#ifdef ILODEBUG
void ilchkopt(struct img *img,enum ilopt opt,char act){
	if(!img->il) return;
	if(act==ilofind(img->il->opt[opt],img)) return;
	ilodbg(img,opt,act?"ERR-miss":"ERR-exist");
}
#else
void ilchkopt(struct img *UNUSED(img),enum ilopt UNUSED(opt),char UNUSED(act)){ }
#endif
