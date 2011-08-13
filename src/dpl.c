#include "config.h"
#include <stdlib.h>
#include <SDL.h>
#include <math.h>
#include <unistd.h> // rename, rmdir
#include "dpl_int.h"
#include "sdl.h"
#include "load.h"
#include "main.h"
#include "cfg.h"
#include "exif.h"
#include "act.h"
#include "file.h"
#include "eff_int.h"
#include "gl.h"
#include "pano.h"
#include "mark.h"
#include "help.h"
#include "map.h"

#define INPUTTXTBACK	0x80
#define INPUTTXTTAB 	0x81

enum colmode { COL_NONE=-1, COL_G=0, COL_C=1, COL_B=2 };

enum dplevgrp { DEG_RIGHT, DEG_LEFT, DEG_ZOOMIN, DEG_ZOOMOUT, DEG_PLAY, DEG_NUM, DEG_NONE };

enum inputtxt {ITM_OFF, ITM_CATSEL, ITM_TXTIMG, ITM_NUM, ITM_SEARCH, ITM_DIRED, ITM_MAPMK};

const char *colmodestr[]={"G","B","C"};

struct dpl {
	struct dplpos pos;
	char writemode;
	Uint32 run;
	struct {
		Uint32 displayduration;
		Uint32 holdduration;
		char loop;
		float prged_w;
		char playmode;
		char playrecord;
		unsigned int playrecord_rate;
	} cfg;
	struct dplinput input;
	enum colmode colmode;
	Uint32 evdelay[DEG_NUM];
	int actimgi;
	unsigned int fid;
	struct imglist *resortil;
	unsigned int infosel;
	enum diredmode {DEM_FROM,DEM_TO,DEM_ALL} diredmode;
	struct dplmousehold {
		int imgi;
		Uint32 time;
	} mousehold;
} dpl = {
	.pos.imgi = { IMGI_START },
	.pos.zoom = 0,
	.pos.x = 0.,
	.pos.y = 0.,
	.writemode = 0,
	.run = 0,
	.colmode = COL_NONE,
	.pos.actil = 0,
	.input.mode = ITM_OFF,
	.fid = 0,
	.resortil = NULL,
	.mousehold.time = 0,
};

#define AIL			(dpl.pos.actil&ACTIL)
#define AIMGI		(dpl.pos.imgi[AIL])
#define AIL_LEFT	((dpl.pos.actil&ACTIL_ED) && AIL==0)
//#define AIL_RIGHT	((dpl.pos.actil&ACTIL_ED) && AIL==1)

/***************************** dpl interface **********************************/

/* thread: all */
struct dplpos *dplgetpos(){ return &dpl.pos; }
int dplgetimgi(int il){ if(il<0 || il>=IL_NUM) il=AIL; return dpl.pos.imgi[il]; }
int dplgetzoom(){ return dpl.pos.zoom; }
char dplloop(){ return dpl.cfg.loop; }
struct dplinput *dplgetinput(){
	if(dpl.input.mode==ITM_OFF) return NULL;
	if(dpl.input.in[0]=='\0'){
		static struct dplinput in={ .in={'\0'}, .post={'\0'}, .id=-1 };
		switch(dpl.input.mode){
		case ITM_CATSEL: snprintf(in.pre,FILELEN,_("[Catalog]")); break;
		case ITM_TXTIMG: snprintf(in.pre,FILELEN,_("[Text]")); break;
		case ITM_NUM:    snprintf(in.pre,FILELEN,_("[Number]")); break;
		case ITM_SEARCH: snprintf(in.pre,FILELEN,_("[Search]")); break;
		case ITM_DIRED:
		case ITM_MAPMK:  snprintf(in.pre,FILELEN,_("[Directory]")); break;
		}
		return &in;
	}
	return &dpl.input;
}
int dplgetactil(char *prged){
	if(prged) *prged=(dpl.pos.actil&ACTIL_PRGED)!=0;
	return (dpl.pos.actil&ACTIL_ED) ? AIL : -1;
}
int dplgetactimgi(int il){ return (AIL==il && (dpl.pos.actil&ACTIL_PRGED)) ? dpl.actimgi : -1; }

/* thread: sdl */
unsigned int dplgetfid(){ return dpl.fid++; }

/* thread: load */
void dplsetresortil(struct imglist *il){ dpl.resortil=il; }

char dplwritemode(){
	return dpl.writemode || (dpl.pos.actil&ACTIL_ED);
}

Uint32 dplgetticks(){
	if(dpl.cfg.playrecord) return dpl.fid*1000/dpl.cfg.playrecord_rate;
	return SDL_GetTicks();
}

char *dplgetinfo(unsigned int *sel){
	struct img *img;
	if(!(img=imgget(AIL,AIMGI))) return NULL;
	if(sel) *sel=dpl.infosel;
	return imgexifinfo(img->exif);
}

/***************************** imgfit *****************************************/

char imgfit(struct img *img,float *fitw,float *fith){
	float irat;
	float srat=sdlrat();
	enum rot rot;
	if(!img || !(irat=imgldrat(img->ld))) return 0;
	rot=imgexifrot(img->exif);
	if(rot==ROT_90 || rot==ROT_270) irat=1.f/irat;
	if(fitw) *fitw = srat>irat ? irat/srat : 1.f;
	if(fith) *fith = srat>irat ? 1.f : srat/irat;
	return 1;
}

char imgspos2ipos(struct img *img,float sx,float sy,float *ix,float *iy){
	float fitw,fith;
	if(dpl.pos.zoom<0) return 0;
	if(panospos2ipos(img,sx,sy,ix,iy)) return 1;
	if(!imgfit(img,&fitw,&fith)) return 0;
	fitw*=powf(2.f,(float)dpl.pos.zoom);
	fith*=powf(2.f,(float)dpl.pos.zoom);
	if(ix) *ix = sx/fitw;
	if(iy) *iy = sy/fith;
	return 1;
}

void printixy(float sx,float sy){
	struct img *img=imgget(AIL,AIMGI);
	float ix,iy;
	if(!img || dpl.pos.zoom<0) return;
	if(!(imgspos2ipos(img,sx,sy,&ix,&iy))) return;
	debug(DBG_NONE,"img pos %.3fx%.3f",ix+dpl.pos.x,iy+dpl.pos.y);
}

/***************************** dpl img move ***********************************/

struct zoomtab {
	int move;
	float size;
	int inc;
	enum imgtex texcur;
	enum imgtex texoth;
} zoomtab[]={
	{ .move=5, .size=1.f,     .inc=0,  .texcur=TEX_BIG,   .texoth=TEX_BIG,   },
	{ .move=3, .size=1.f/3.f, .inc=4,  .texcur=TEX_SMALL, .texoth=TEX_SMALL, },
	{ .move=5, .size=1.f/5.f, .inc=12, .texcur=TEX_SMALL, .texoth=TEX_TINY,  },
	{ .move=7, .size=1.f/7.f, .inc=24, .texcur=TEX_TINY,  .texoth=TEX_TINY,  },
};

void dplclippos(struct img *img){
	float xb,yb;
	float x[2],y[2];
	char clipx;
	if(!imgspos2ipos(img,.5f,.5f,&xb,&yb)) return;
	clipx=panoclip(img,&xb,&yb);
	y[0]=-.5f+yb; y[1]= .5f-yb;
	if(y[1]<y[0]) y[0]=y[1]=0.f;
	if(dpl.pos.y<y[0]) dpl.pos.y=y[0];
	if(dpl.pos.y>y[1]) dpl.pos.y=y[1];
	if(!clipx) return;
	x[0]=-.5f+xb; x[1]= .5f-xb;
	if(x[1]<x[0]) x[0]=x[1]=0.f;
	if(dpl.pos.x<x[0]){ dpl.pos.x=x[0]; panoev(PE_FLIPRIGHT); }
	if(dpl.pos.x>x[1]){ dpl.pos.x=x[1]; panoev(PE_FLIPLEFT); }
}

char dplmovepos(float sx,float sy){
	struct img *img;
	if(dpl.pos.actil&ACTIL_PRGED){
		struct ecur *ecur;
		if(AIL!=1 || !(img=imgget(1,dpl.actimgi))) return 0;
		sx/=1.f-dpl.cfg.prged_w;
		ecur=imgposcur(img->pos);
		ecur->x-=sx;
		ecur->y-=sy;
		return 0;
	}else if(mapon()){
		return mapmovepos(sx,sy);
	}else{
		float ix,iy;
		if(!(img=imgget(AIL,AIMGI)) || !imgspos2ipos(img,sx,sy,&ix,&iy)) return 0;
		panotrimmovepos(img,&ix,&iy);
		dpl.pos.x+=ix;
		dpl.pos.y+=iy;
		dplclippos(img);
		debug(DBG_DBG,"dpl move pos => imgi %i zoom %i pos %.2fx%.2f",AIMGI,dpl.pos.zoom,dpl.pos.x,dpl.pos.y);
		return 1;
	}
}

void dplzoompos(int nzoom,float sx,float sy){
	float oix,oiy,nix,niy;
	char o,n;
	struct img *img=imgget(AIL,AIMGI);
	o=imgspos2ipos(img,sx,sy,&oix,&oiy);
	dpl.pos.zoom=nzoom;
	n=imgspos2ipos(img,sx,sy,&nix,&niy);
	if(o && n){
		dpl.pos.x+=oix-nix;
		dpl.pos.y+=oiy-niy;
	}
	if(img) dplclippos(img);
}

void dplclipimgi(int *imgi){
	int i = imgi ? *imgi : AIMGI;
	int nimg=imggetn(AIL);
	if(dpl.cfg.loop){
		if(i==IMGI_START) i=0;
		if(i==IMGI_END)   i=nimg-1;
		while(i<0)     i+=nimg;
		while(i>=nimg) i-=nimg;
	}else if(imgi){
		/* no change for dplmark without loop */
	}else if(dpl.pos.zoom<0){
		if(i<0)     i=0;
		if(i>=nimg) i=nimg-1;
	}else if(i!=IMGI_START && i!=IMGI_END){
		if(i<0)     i=0;
		if(i>=nimg) i=IMGI_END;
	}
	if(imgi) *imgi=i; else AIMGI=i;
}

void dplsecdir(){
	struct imglist *il;
	struct img *img;
	const char *dir;
	int ail;
	if(dpl.pos.actil!=ACTIL_DIRED) return;
	if(!(img=imgget(0,dpl.pos.imgi[0]))) return;
	if(!(dir=imgfiledir(img->file))) return;
	if(!(il=floaddir(imgfilefn(img->file),dir))) return;
	if(il==ilget(1)) return;
	ilswitch(il,1);
	ail=dpl.pos.actil;
	dpl.pos.actil|=1;
	dplclipimgi(NULL);
	effinit(EFFREF_CLR,0,-1);
	dpl.pos.actil=ail;
}

void dplchgimgi(int dir){
	AIMGI=imginarrorlimits(AIL,AIMGI)+dir;
}

int dplclickimg(float sx,float sy,int evimgi){
	int i,x,y;
	if(evimgi>=-1)         return evimgi;
	if(mapon())            return -1;
	if(AIMGI==IMGI_START)  return IMGI_START;
	if(AIMGI==IMGI_END)    return IMGI_END;
	if(dpl.pos.zoom>=0)    return AIMGI;
	sx/=effmaxfit().w; if(sx> .49f) sx= .49f; if(sx<-.49f) sx=-.49f;
	sy/=effmaxfit().h; if(sy> .49f) sy= .49f; if(sy<-.49f) sy=-.49f;
	x=(int)floorf(sx/zoomtab[-dpl.pos.zoom].size+.5f);
	y=(int)floorf(sy/zoomtab[-dpl.pos.zoom].size+.5f);
	i=(int)floorf((float)y/zoomtab[-dpl.pos.zoom].size+.5f);
	i+=x;
	return AIMGI+i;
}

char dplprged(const char *cmd,int il,int imgi,int arg){
	struct img *img=NULL;
	struct prg *prg;
	char buf[FILELEN*2];
	if(!(dpl.pos.actil&ACTIL_PRGED)) return 0;
	if(il>=0 && AIL!=il) return 0;
	if(il<0) il=-il-1;
	if(!(prg=ilprg(1))) return 0;
	if(imgi>=0 && !(img=imgget(il,imgi))) return 0;
	if(img && imgfiledir(img->file)) return 0;
	if(cmd){
		const char *fn="";
		struct txtimg *txt=NULL;
		if(img) fn=imgfilefn(img->file);
		if(!strcmp(cmd,"txtadd")) fn=dpl.input.in;
		if(img && (txt=imgfiletxt(img->file))) fn=txt->txt;
		snprintf(buf,FILELEN*2,"%s \"%s\" %i",cmd,fn,dpl.pos.imgi[1]);
		if(img && !strcmp(cmd,"imgpos")){
			size_t len=strlen(buf);
			struct ecur *ecur=imgposcur(img->pos);
			snprintf(buf+len,FILELEN*2-len," :%.3f:%.3f:%.3f",
				ecur->s,ecur->x,ecur->y);
		}
		if(!strcmp(cmd,"frmmov") || !strcmp(cmd,"frmcpy")){
			size_t len=strlen(buf);
			if(arg==-1) arg=dpl.pos.imgi[1]+1;
			else if(arg==-2) arg=dpl.pos.imgi[1]-1;
			else arg--;
			snprintf(buf+len,FILELEN*2-len," %i",arg);
		}
		if(img && !strcmp(cmd,"imgon")){
			size_t len=strlen(buf);
			snprintf(buf+len,FILELEN*2-len," ::%i",imgposopt(img->pos)->layer);
		}
		if(txt && (!strcmp(cmd,"imgcol") || !strcmp(cmd,"frmcol"))){
			size_t len=strlen(buf);
			snprintf(buf+len,FILELEN*2-len," 0x%02x%02x%02x",
					(int)(txt->col[0]*255.f),
					(int)(txt->col[1]*255.f),
					(int)(txt->col[2]*255.f));
		}
	}
	if(!ilreload(1,cmd?buf:NULL)) return 1;
	effprgcolinit(NULL,-2);
	il=AIL;
	if(il!=1) dpl.pos.actil=ACTIL_PRGED|1;
	effinit(EFFREF_ALL,strcmp(cmd,"reload")?DE_JUMP:0,-1);
	if(il!=1) dpl.pos.actil=ACTIL_PRGED|il;
	return 1;
}

void dpllayer(char dir,int imgi){
	struct img *img=imgget(1,imgi);
	if(!img) return;
	if(dir>0) imgposopt(img->pos)->layer++;
	else      imgposopt(img->pos)->layer--;
	dplprged("imgon",-2,imgi,-1);
}

void dplprgcolreset(){
	int saveimgi;
	if(!(dpl.pos.actil&ACTIL_PRGED)) return;
	saveimgi=effprgcolinit(NULL,-1);
	if(saveimgi>=0) dplprged("imgcol",1,saveimgi,-1);
}

char dplprgcol(){
	struct img *img;
	struct txtimg *txt=NULL;
	int saveimgi;
	int initimgi=dpl.actimgi;
	if(!(dpl.pos.actil&ACTIL_PRGED)) return 0;
	if(AIL!=1 || !(img=imgget(1,dpl.actimgi)) || !(txt=imgfiletxt(img->file)))
		initimgi=-1;
	saveimgi=effprgcolinit(txt?txt->col:NULL,initimgi);
	if(saveimgi>=0) dplprged("imgcol",1,saveimgi,-1);
	return 1;
}

char dplprgcolcopy(){
	int imgi=effprgcolinit(NULL,-1);
	struct img *img;
	if(!(dpl.pos.actil&ACTIL_PRGED)) return 0;
	if(imgi<0)  imgi=dpl.actimgi;
	if(AIL==1 && (img=imgget(1,imgi)) && imgfiletxt(img->file))
		dplprged("frmcol", 1,imgi,-1);
	return 1;
}

void dplmove(enum dplev ev,float sx,float sy,int clickimg){
	static const int zoommin=sizeof(zoomtab)/sizeof(struct zoomtab);
	int dir=DE_DIR(ev);
	if(mapmove(ev,sx,sy)) return;
	if(ev&DE_MOVE) dplmovepos(sx,sy);
	if(ev&(DE_RIGHT|DE_LEFT)){
		if(!panoev(dir<0?PE_SPEEDLEFT:PE_SPEEDRIGHT)){
			if(dpl.pos.zoom<=0) dplchgimgi(dir);
			else dplmovepos((float)dir*.25f,0.f);
		}
	}
	if(ev&(DE_UP|DE_DOWN)){
		if(AIL_LEFT) dplchgimgi(-dir);
		else if(dpl.pos.zoom<0)  dplchgimgi(-dir*zoomtab[-dpl.pos.zoom].move);
		else if(dpl.pos.zoom==0) dplchgimgi( dir*zoomtab[-dpl.pos.zoom].move);
		else dplmovepos(0.f,-(float)dir*.25f);
	}
	if(ev&(DE_ZOOMIN|DE_ZOOMOUT)){
		float x,fitw;
		struct img *img;
		if(dpl.pos.actil&ACTIL_PRGED){
			if((img=imgget(1,dpl.actimgi))){
				imgposcur(img->pos)->s *= dir>0 ? sqrtf(2.f) : sqrtf(.5f);
				dplprged("imgpos",1,dpl.actimgi,-1);
			}
			return;
		}
		if((dpl.pos.actil&ACTIL_DIRED) && AIL==0) return;
		if((dpl.pos.actil&ACTIL_DIRED) && dir>0 && dpl.pos.zoom>=-1) return;
		if(dpl.pos.zoom<0 && clickimg>=0){
			AIMGI=clickimg;
			dplclipimgi(NULL);
		}
		img=imgget(AIL,AIMGI);
		if(dpl.pos.zoom==0 && dir>0 && img && imgfiledir(img->file)) return;
		if(dpl.pos.zoom==0 && dir>0 && imgfit(img,&fitw,NULL) && panostart(img,fitw,&x)){
			dpl.pos.x=x;
			dpl.pos.zoom+=dir;
			dplclippos(img);
		}else if(dpl.pos.zoom+dir<=0){
			if(dpl.pos.zoom==1) effpanoend(img);
			dpl.pos.x=dpl.pos.y=0.;
			dpl.pos.zoom+=dir;
		}else dplzoompos(dpl.pos.zoom+dir,sx,sy);
	}
	if(dpl.pos.zoom<1-zoommin) dpl.pos.zoom=1-zoommin;
	if(dpl.pos.zoom>0)    dpl.run=0;
	dplclipimgi(NULL);
	if((AIMGI==IMGI_START || AIMGI==IMGI_END) && dpl.pos.zoom>0) dpl.pos.zoom=0;
	debug(DBG_STA,"dpl move => imgi %i zoom %i pos %.2fx%.2f",AIMGI,dpl.pos.zoom,dpl.pos.x,dpl.pos.y);
	effinit(EFFREF_ALL|EFFREF_FIT,ev,-1);
	dplsecdir();
}

void dplsel(int imgi){
	if(imgi<0 || imgi==IMGI_START || imgi==IMGI_END) return;
	AIMGI=imgi;
	dplclipimgi(NULL);
	effinit(EFFREF_ALL|EFFREF_FIT,DE_SEL,-1);
	dplsecdir();
}

char dplmark(int imgi){
	struct img *img;
	char *mark;
	size_t mkid=0;
	if(!dplwritemode()) return 0;
	if(imgi==IMGI_START || imgi==IMGI_END) return 0;
	if(imgi>=IMGI_CAT){
		mkid=(size_t)imgi-IMGI_CAT+1;
		imgi=AIMGI;
		if(imgi==IMGI_START || imgi==IMGI_END) return 0;
		if(mkid>markncat()) return 0;
	}
	dplclipimgi(&imgi);
	if(!(img=imgget(AIL,imgi))) return 0;
	if(imgfiledir(img->file)) return 0;
	mark=imgposmark(img,MPC_ALLWAYS);
	mark[mkid]=!mark[mkid];
	effinit(EFFREF_IMG,DE_MARK,imgi);
	markchange(mkid);
	actadd(ACT_SAVEMARKS,NULL);
	return 1;
}

void dplrotate(enum dplev ev){
	struct img *img=imgget(AIL,AIMGI);
	int r=DE_DIR(ev);
	if(!img) return;
	if(imgfiledir(img->file)) return;
	exifrotate(img->exif,r);
	effinit(EFFREF_IMG|EFFREF_FIT,ev,-1);
	if(dplwritemode()) actadd(ACT_ROTATE,img);
}

enum edpldel { DD_DEL, DD_ORI };
void dpldel(enum edpldel act){
	struct img *img=NULL;
	if(!dplwritemode()) return;
	if(act==DD_ORI && (img=imgget(AIL,AIMGI)) && fimgswitchmod(img)){
		effinit(EFFREF_IMG|EFFREF_FIT,0,-1);
	}else if((img=imgdel(AIL,AIMGI))){
		if(dpl.pos.zoom>0) dpl.pos.zoom=0;
		if(delimg){
			struct img *tmp=delimg;
			delimg=NULL;
			ldffree(tmp->ld,TEX_NONE);
		}
		dpl.pos.imgiold--;
		effdel(img->pos);
		effinit(EFFREF_ALL|EFFREF_FIT,DE_RIGHT,-1);
		delimg=img;
		dplclipimgi(NULL);
	}else return;
	actadd(act==DD_ORI ? ACT_DELORI : ACT_DELETE,img);
}

char dplcol(int d){
	struct img *img;
	float *val;
	if(dpl.colmode==COL_NONE) return 0;
	if(!(img=imgget(AIL,AIMGI))) return 0;
	if(imgfiledir(img->file)) return 0;
	val=((float*)imgposcol(img->pos))+dpl.colmode;
	*val+=.1f*(float)d;
	if(*val<-1.f) *val=-1.f;
	if(*val> 1.f) *val= 1.f;
	return 1;
}

char dpldir(int imgi,char noleave){
	struct img *img;
	const char *fn=NULL;
	const char *dir=NULL;
	struct imglist *il=NULL;
	if(AIL!=0) return 0;
	if(imgi>=IMGI_CAT && imgi<IMGI_END && !(fn=markcatfn(imgi-IMGI_CAT,&dir))) return 1;
	if(imgi>=IMGI_MAP && imgi<IMGI_CAT && !mapgetclt(imgi-IMGI_MAP,&il,&fn,&dir)) return 1;
	if(imgi==IMGI_START) return 0;
	if(!il && !fn && !(img=imgget(AIL,imgi))){
		if(noleave) return 0;
		imgi=ilswitch(NULL,0);
		if(imgi==IMGI_END) return 1;
	}else{
		if(!il){
			if(!fn){
				if(!(dir=imgfiledir(img->file))) return 0;
				fn=imgfilefn(img->file);
				AIMGI=imgi;
			}else actforce();
			if(!(il=floaddir(fn,dir))) return 1;
		}
		if(il==ilget(0)) return 1;
		imgi=ilswitch(il,0);
		if(imgi==IMGI_START && !ilprg(0)) imgi=0;
		if(ilprg(0)){ dpl.pos.zoom=0; imgi=IMGI_START; }
	}
	dpl.run=0;
	AIMGI=imgi;
	dpl.pos.imgiold=imgi;
	effinit(EFFREF_CLR,0,-1);
	sdlforceredraw(); /* TODO remove (for switch to map which does not use eff) */
	dplsecdir();
	return 1;
}

#define ADDTXT(...)	{ snprintf(txt,(size_t)(dsttxt+ISTAT_TXTSIZE-txt),__VA_ARGS__); txt+=strlen(txt); }
void dplstatupdate(){
	char *dsttxt=effstat()->txt;
	char run=dpl.run!=0;
	if(mapstatupdate(dsttxt)) goto end;
	if(AIMGI==IMGI_START) snprintf(dsttxt,ISTAT_TXTSIZE,_("BEGIN"));
	else if(AIMGI==IMGI_END) snprintf(dsttxt,ISTAT_TXTSIZE,_("END"));
	else{
		struct img *img=imgget(AIL,AIMGI);
		struct icol *ic;
		char *txt=dsttxt;
		const char *tmp;
		if(!img) return;
		ic=imgposcol(img->pos);
		ADDTXT("%i/%i ",AIMGI+1,imggetn(AIL));
		if(!ilprg(AIL) || dpl.pos.zoom!=0){
			snprintf(txt,(size_t)(dsttxt+ISTAT_TXTSIZE-txt),imgfilefn(img->file));
			utf8check(txt);
			txt+=strlen(txt);
		}
		switch(imgexifrot(img->exif)){
			case ROT_0: break;
			case ROT_90:  ADDTXT(_(" rotated-right")); break;
			case ROT_180: ADDTXT(_(" rotated-twice")); break;
			case ROT_270: ADDTXT(_(" rotated-left")); break;
		}
		if(dplwritemode()){
			char *mark;
			ADDTXT(_(" (write-mode)"));
			if((mark=imgposmark(img,MPC_NO)) && mark[0]) ADDTXT(_(" [MARK]"));
		}
		if((tmp=ilsortget(AIL))) ADDTXT(" sort:%s",tmp);
		if(dpl.colmode!=COL_NONE || ic->g || ic->c || ic->b){
			ADDTXT(" %sG:%.1f%s",dpl.colmode==COL_G?"[":"",ic->g,dpl.colmode==COL_G?"]":"");
			ADDTXT(" %sC:%.1f%s",dpl.colmode==COL_C?"[":"",ic->c,dpl.colmode==COL_C?"]":"");
			ADDTXT(" %sB:%.1f%s",dpl.colmode==COL_B?"[":"",ic->b,dpl.colmode==COL_B?"]":"");
		}
		run|=panostattxt(txt,(size_t)(dsttxt-ISTAT_TXTSIZE-txt));
	}
	effstat()->run=run;
end:
	sdlforceredraw();
}

void dplinputtxtfinal(char ok);

char dplactil(float x,int clickimg){
	int actil;
	if(!(dpl.pos.actil&ACTIL_ED)) return 0;
	actil = (x+.5f>dpl.cfg.prged_w) | (dpl.pos.actil&ACTIL_ED);
	if(actil!=dpl.pos.actil && dpl.input.mode==ITM_SEARCH) dplinputtxtfinal(1);
	dpl.pos.actil=actil;
	if(dpl.pos.actil&ACTIL_PRGED) dpl.actimgi=clickimg;
	sdlforceredraw();
	return 1;
}

int dplcmdrun(void *arg){
	system(arg);
	free(arg);
	return 0;
}

void dplgimp(){
	struct img *img=imgget(AIL,AIMGI);
	char *cmd;
	if(!img) return;
	if(imgfiledir(img->file)) return;
	sdlfullscreen(0);
	cmd=malloc(FILELEN*8);
	snprintf(cmd,FILELEN+8,"gimp %s",imgfilefn(img->file));
	SDL_CreateThread(dplcmdrun,cmd);
}

void dplconvert(){
	struct img *img=imgget(AIL,AIMGI);
	char *cmd;
	if(!img) return;
	if(isdir(imgfilefn(img->file))>1) return;
	sdlfullscreen(0);
	cmd=malloc(FILELEN*8);
	snprintf(cmd,FILELEN+8,"cnv_ui -img %s",imgfilefn(img->file));
	SDL_CreateThread(dplcmdrun,cmd);
}

void dplswloop(){
	dpl.cfg.loop=!dpl.cfg.loop;
	effinit(EFFREF_ALL,0,-1);
}

void dpledit(){
	enum ilsecswitch type;
	if(dpl.pos.actil&ACTIL_ED){
		if(dpl.pos.actil&ACTIL_PRGED) type=ILSS_PRGOFF;
		else if(dpl.pos.actil&ACTIL_DIRED) type=ILSS_DIROFF;
		else return;
	}else{
		if(ilprg(0)) type=ILSS_PRGON;
		else type=ILSS_DIRON;
	}
	if(!ilsecswitch(type)) return;
	dpl.run=0;
	switch(type){
	case ILSS_PRGOFF:
		dpl.pos.imgi[0]=dpl.pos.imgi[1];
	case ILSS_DIROFF:
		dpl.pos.actil=0;
		effinit(EFFREF_ALL,0,-1);
	break;
	case ILSS_PRGON:
		dpl.pos.imgi[1]=dpl.pos.imgi[0];
		dpl.pos.imgi[0]=0;
		dpl.actimgi=-1;
		dpl.pos.zoom=0;
		dpl.pos.actil=ACTIL_PRGED;
		effinit(EFFREF_CLR,0,-1);
		dpl.pos.actil|=1;
		effinit(EFFREF_ALL,0,-1);
	break;
	case ILSS_DIRON:
		dpl.pos.actil=ACTIL_DIRED;
		dpl.pos.zoom=-2;
		dplclipimgi(NULL);
		effinit(EFFREF_CLR,0,-1);
		dplsecdir();
	break;
	}
}

void dpldired(char *input,int id){
	char updatedirs=0;
	const char *dstdir,*srcdir;
	struct img *img,*imgend;
	struct imglist *il;
	char dstbuf[FILELEN],srctdir[FILELEN],dsttdir[FILELEN];
	int actil;
	size_t i;
	if(!(img=imgget(0,dpl.pos.imgi[0]))) return;
	srcdir=imgfilefn(img->file);
//	if(!dir(srcdir)) return;
	if(dpl.diredmode==DEM_FROM && dpl.pos.imgi[1]<=0) dpl.diredmode=DEM_ALL;
	if(dpl.diredmode==DEM_TO   && dpl.pos.imgi[1]>=imggetn(1)-1) dpl.diredmode=DEM_ALL;
	if(id<0){
		updatedirs=1;
		if(!(dstdir=ilfn(ilget(0)))) return;
		snprintf(dstbuf,FILELEN,"%s/%s",dstdir,input);
		dstdir=dstbuf;
		//if(-e dstdir) return;
	}else{
		if(!(img=imgget(0,id))) return;
		dstdir=imgfilefn(img->file);
		// if(!dir(dstdir)) return;
	}
	dsttdir[0]='\0';
	for(i=strlen(srcdir);i--;) if((srcdir[i]=='/' || srcdir[i]=='\\') && !strncmp(srcdir,dstdir,i+1)){
		memcpy(srctdir,srcdir,i);
		snprintf(srctdir+i,FILELEN-i,"/thumb/%s",srcdir+i+1);
		if(isdir(srctdir)==1){
			memcpy(dsttdir,srcdir,i);
			snprintf(dsttdir+i,FILELEN-i,"/thumb/%s",dstdir+i+1);
			break;
		}
	}
	if(dpl.diredmode==DEM_ALL && id<0){
		if(!rename(srcdir,dstdir)){
			updatedirs=1;
			if(dsttdir[0]) rename(srctdir,dsttdir);
		}
	}else{
		if(id<0 && !mkdirm(dstdir)) return;
		if(dsttdir[0] && !isdir(dsttdir)) mkdirm(dsttdir);
		img=imgget(1,dpl.diredmode==DEM_FROM ? dpl.pos.imgi[1] : 0);
		imgend=dpl.diredmode==DEM_TO ? imgget(1,dpl.pos.imgi[1]) : NULL;
		for(;img;img=img->nxt){
			const char *fn=imgfilefn(img->file);
			const char *base=strrchr(fn,'/');
			char imgbuf[FILELEN];
			if(!base) base=fn;
			snprintf(imgbuf,FILELEN,"%s/%s",dstdir,base);
			rename(fn,imgbuf);
			if(dsttdir[0] && imgfiletfn(img->file,&fn)){
				snprintf(imgbuf,FILELEN,"%s/%s",dsttdir,base);
				rename(fn,imgbuf);
			}
			if(img==imgend) break;
		}
		if(dpl.diredmode==DEM_ALL){
			if(!rmdir(srcdir)) updatedirs=1;
			if(dsttdir[0]) rmdir(srctdir);
		}
	}
	actil=dpl.pos.actil;
	dpl.pos.actil&=~ACTIL;
	if(updatedirs && (il=floaddir(ilfn(ilget(0)),ildir()))){
		ilswitch(il,0);
		dplclipimgi(NULL);
		effinit(EFFREF_ALL,DE_INIT,-1);
	}
	dplsecdir();
	dpl.pos.actil=actil;
}

void dplmap(){
	if(mapon()) mapswtype(); else{
		struct imglist *il=mapsetpos(dpl.pos.imgi[0]);
		ilswitch(il,0);
		effinit(EFFREF_ALL,0,-1);
		sdlforceredraw();
	}
}

void dpljump(int mid,float sx,float sy,int imgi,enum dplev ev){
	switch(mid){
	case 0: if(dplmovepos(sx,sy)) effinit(EFFREF_IMG,ev,-1); break;
	case 1:
		if(dplwritemode()){
			mapinfo(-1);
			mapcltmove(imgi-IMGI_MAP,sx,sy);
		}
	break;
	}
}

/***************************** dpl action *************************************/

void dplsetdisplayduration(int dur){
	if(dur<100) dur*=1000;
	if(dur>=250 && dur<=30000) dpl.cfg.displayduration=(unsigned int)dur;
}

/***************************** dpl key ****************************************/

#define DPLEVS_NUM	128
#define DPLEV_NMOVE	2
struct dplevs {
	struct ev {
		enum dplev ev;
		unsigned short key;
		float sx,sy;
		int imgi;
		enum dplevsrc src;
	} evs[DPLEVS_NUM];
	struct {
		float sx,sy;
		int imgi;
	} move[DPLEV_NMOVE];
	int wi,ri;
} dev = {
	.wi = 0,
	.ri = 0,
};

/* thread: sdl */
void dplevputx(enum dplev ev,unsigned short key,float sx,float sy,int imgi,enum dplevsrc src){
	if(ev&DE_JUMP){
		if(key<DPLEV_NMOVE){
			dev.move[key].sy+=sy;
			dev.move[key].sx+=sx;
			dev.move[key].imgi=imgi;
		}
		ev&=~(unsigned int)DE_JUMP;
	}
	if(ev){
		int nwi=(dev.wi+1)%DPLEVS_NUM;
		dev.evs[dev.wi].ev=ev;
		dev.evs[dev.wi].key=key;
		dev.evs[dev.wi].sx=sx;
		dev.evs[dev.wi].sy=sy;
		dev.evs[dev.wi].imgi=imgi;
		dev.evs[dev.wi].src=src;
		if(nwi!=dev.ri) dev.wi=nwi;
	}
}

#include "dpl_help.h"
#define IF_WRM
#include "dpl_help.h"
#undef  IF_WRM
#define IF_PRGED
#include "dpl_help.h"
#undef  IF_PRGED
#define IF_DIRED
#include "dpl_help.h"
#undef  IF_DIRED

/* thread: gl */
const char *dplhelp(){
	if(dpl.pos.actil&ACTIL_PRGED) return dlphelp_prged;
	if(dpl.pos.actil&ACTIL_DIRED) return dlphelp_dired;
	if(dpl.writemode)             return dlphelp_wrm;
	return dlphelp_def;
}

void dplmousehold(int clickimg){
	if(!mapon() || clickimg<IMGI_MAP || clickimg>=IMGI_CAT || dpl.input.mode!=ITM_OFF){
		mapinfo(-1);
		dpl.mousehold.time=0;
	}else{
		dpl.mousehold.imgi=clickimg-IMGI_MAP;
		dpl.mousehold.time=SDL_GetTicks()+dpl.cfg.holdduration;
	}
}

void dplmouseholdchk(){
	if(!dpl.mousehold.time || dpl.mousehold.time>SDL_GetTicks()) return;
	mapinfo(dpl.mousehold.imgi);
	dpl.mousehold.time=0;
}

void dplfilesearch(struct dplinput *in,int il){
	struct img *img=imgget(il,0);
	int i=0;
	size_t ilen=strlen(in->in);
	if(in->in[0]) for(;img;img=img->nxt,i++){
		const char *fn=imgfilefn(img->file);
		const char *pos=strrchr(fn,'/');
		size_t plen,p;
		if(pos) pos++; else pos=fn;
		if((plen=strlen(pos))<ilen) continue;
		for(p=0;p<=plen-ilen;p++){
			if(strncasecmp(pos+p,in->in,ilen)) continue;
			memcpy(in->pre,pos,p);
			in->pre[p]='\0';
			memcpy(in->in,pos+p,ilen);
			memcpy(in->post,pos+p+ilen,plen-p-ilen);
			in->post[plen-p-ilen]='\0';
			if(dpl.input.mode==ITM_SEARCH) dplsel(i);
			else dpl.input.id=i;
			return;
		}
	}
	in->pre[0]='\0';
	in->post[0]='\0';
	if(dpl.input.mode==ITM_SEARCH) dplsel(in->id);
	else dpl.input.id=-1;
}

void dplmapsearch(struct dplinput *in){
	const char *bdir=mapgetbasedirs();
	int id;
	size_t len=strlen(in->in);
	in->id=-1;
	in->pre[0]='\0';
	in->post[0]='\0';
	in->res[0]='\0';
	if(bdir) for(id=0;bdir[0];bdir+=FILELEN*2,id++){
		const char *bname=bdir+FILELEN;
		size_t l=strlen(bname);
		if(!l){
			finddirmatch(in->in,in->post,in->res,bdir);
			if(in->post[0]) break;
		}else if(len<=l && !strncmp(bname,in->in,len) && (l==len || strncmp(in->post,bname+len,l-len)<0)){
			snprintf(in->post,MAX(FILELEN,l-len),bname+len);
			snprintf(in->res,FILELEN,bdir);
		}else if(len>l && !strncmp(bname,in->in,l) && (in->in[l]=='/' || in->in[l]=='\\')){
			finddirmatch(in->in+l+1,in->post,in->res,bdir);
			if(in->post[0]) break;
		}
	}
}

void dplinputtxtadd(uint32_t c){
	size_t len=strlen(dpl.input.in);
	switch(c){
	case INPUTTXTBACK:
		while(len && (dpl.input.in[len-1]&0xc0)==0x80) len--;
		if(len) len--;
	break;
	case INPUTTXTTAB:
		if(dpl.input.pre[0] || dpl.input.post[0]){
			char buf[FILELEN];
			snprintf(buf,FILELEN,"%s%s%s",dpl.input.pre,dpl.input.in,dpl.input.post);
			snprintf(dpl.input.in,FILELEN,buf);
			len=strlen(dpl.input.in);
		}
	break;
	default:
	{
		char *buf=(char *)&c;
		int i;
		for(i=0;i<4 && buf[i];i++) dpl.input.in[len++]=buf[i];
	}
	break;
	}
	dpl.input.in[len]='\0';
	switch(dpl.input.mode){
	case ITM_CATSEL: markcatsel(&dpl.input); break;
	case ITM_SEARCH: if(!mapsearch(&dpl.input)) dplfilesearch(&dpl.input,AIL); break;
	case ITM_DIRED:  dplfilesearch(&dpl.input,0);   break;
	case ITM_MAPMK:  dplmapsearch(&dpl.input); break;
	}
	sdlforceredraw();
}

#define dplinputtxtinit(mode)	dplinputtxtinitp(mode,0.f,0.f)
char dplinputtxtinitp(enum inputtxt mode,float x,float y){
	if(mode==ITM_SEARCH && ilprg(AIL)) return 0;
	if(mode==ITM_DIRED  && !(dpl.pos.actil&ACTIL_DIRED)) return 0;
	if(mode==ITM_MAPMK  && (!dpl.writemode || !mapon())) return 0;
	if(mode==ITM_SEARCH) mapsavepos();
	if(mode==ITM_SEARCH || mode==ITM_MAPMK) mapinfo(-1);
	dpl.input.pre[0]='\0';
	dpl.input.in[0]='\0';
	dpl.input.post[0]='\0';
	dpl.input.id=-1;
	dpl.input.x=x;
	dpl.input.y=y;
	dpl.input.mode=mode;
	if(mode==ITM_SEARCH) dpl.input.id=AIMGI;
	sdlforceredraw();
	return 1;
}

void dplinputtxtfinal(char ok){
	size_t len=strlen(dpl.input.in);
	switch(dpl.input.mode){
	case ITM_CATSEL: if(ok && dpl.input.id>=0) dplevputi(DE_MARK,dpl.input.id+IMGI_CAT); break;
	case ITM_TXTIMG: if(ok && len) dplprged("txtadd",-1,-1,-1); break;
	case ITM_NUM:    if(ok && len) dplsel(atoi(dpl.input.in)-1); break;
	case ITM_SEARCH: if(!ok && !maprestorepos()) dplsel(dpl.input.id);
	case ITM_DIRED:  if(ok) dpldired(dpl.input.in,dpl.input.id); break;
	case ITM_MAPMK:
		if(ok && dpl.input.res[0]) mapmarkpos(dpl.input.x,dpl.input.y,dpl.input.res);
	break;
	}
	dpl.input.mode=ITM_OFF;
	sdlforceredraw();
}

void dplkey(unsigned short keyu){
	uint32_t key=unicode2utf8(keyu);
	int inputnum=-1;
	if(!key) return;
	debug(DBG_STA,"dpl key 0x%08x",key);
	if(dpl.input.mode!=ITM_OFF){
		if(key<0x20 || key==0x7f) switch(key){
			case 8:  dplinputtxtadd(INPUTTXTBACK); break;
			case 9:  dplinputtxtadd(INPUTTXTTAB); break;
			case 13: dplinputtxtfinal(1); break;
			default: dplinputtxtfinal(0); break;
		}else if(dpl.input.mode!=ITM_NUM || (key>='0' && key<='9')) dplinputtxtadd(key);
		else{
			if(key=='m' || key=='d') inputnum=atoi(dpl.input.in);
			dpl.input.mode=ITM_OFF;
			sdlforceredraw();
		}
		if(inputnum<0) return;
	}
	switch(key){
	case ' ': dplevput(DE_STOP|DE_DIR|DE_PLAY);       break;
	case  27: if(effsw(ESW_CAT,0)) break;
			  if(effsw(ESW_INFO,0)) break;
			  if(effsw(ESW_HELP,0)) break;
	case 'q': sdl_quit=1; break;
	case   8: if(!panoev(PE_MODE)) dplevputi(DE_DIR,IMGI_END); break;
	case 'r': dplrotate(DE_ROT1); break;
	case 'R': dplrotate(DE_ROT2); break;
	case 'p': panoev(PE_FISHMODE); break;
	case 'f': sdlfullscreen(-1); break;
	case 'w':
		if(dpl.pos.actil&ACTIL_ED) dpl.pos.actil=(dpl.pos.actil&~ACTIL)|(!AIL); else{
			dpl.writemode=!dpl.writemode;
			effrefresh(EFFREF_ALL);
		}
	break;
	case 'm':
		if(!dplprged("frmmov",1,-1,inputnum)){
			if(dplinputtxtinit(ITM_DIRED)) dpl.diredmode=DEM_FROM;
			else if(!dplmark(AIMGI)) dplmap();
		}
	break;
	case 'M': if(!dplprged("frmmov",1,-1,-2) && dplinputtxtinit(ITM_DIRED)) dpl.diredmode=DEM_TO; break;
	case 'j': if(dplinputtxtinit(ITM_DIRED)) dpl.diredmode=DEM_ALL; break;
	case 'd': if(!dplprged("frmcpy",1,-1,inputnum) && inputnum>=0) dplsetdisplayduration(inputnum); break;
	case 'g': if(glprg()) dpl.colmode=COL_G; break;
	case 'c': if(!dplprgcol() && glprg()) dpl.colmode=COL_C; break;
	case 'C': if(!dplprgcolcopy()) dplconvert(); break;
	case 'b': if(glprg()) dpl.colmode=COL_B; break;
	case 'k': effsw(ESW_CAT,-1); break;
	case 's': if(dplwritemode()){ dplinputtxtinit(ITM_CATSEL); effsw(ESW_CAT,1); } break;
	case 'S': ilsort(0,NULL,ILSCHG_INC); break;
	case 127: if(dplwritemode()) dpldel(DD_DEL); break;
	case 'o': if(dplwritemode()) dpldel(DD_ORI); break;
	case '+': if(!dplprged("imgadd",-1,!AIL && dpl.actimgi>=0 ? dpl.actimgi : dpl.pos.imgi[0],-1)) dplcol(1); break;
	case '-': if(!dplprged("imgdel", 1,dpl.actimgi,-1)) dplcol(-1); break;
	case 'e': if(!mapeditmode()) dpledit(); break;
	case 'E': if(!dplprged("reload",-1,-1,-1) && ilreload(AIL,NULL)) effinit(EFFREF_ALL,0,-1); break;
	case 'i':
		if(dpl.pos.actil&ACTIL_PRGED) dplprged("frmins",-1,-1,-1);
		else if(dplgetinfo(NULL)) effsw(ESW_INFOSEL,-1);
	break;
	case 'I': if(dplgetinfo(NULL)) effsw(ESW_INFO,-1); break;
	case 'x': dplprged("frmdel",-1,-1,-1); break;
	case 't': if(dpl.pos.actil&ACTIL_PRGED) dplinputtxtinit(ITM_TXTIMG); break;
	case 'l': if(dpl.pos.actil&ACTIL_PRGED) dpllayer(-1,dpl.actimgi); else dplswloop(); break;
	case 'L': if(dpl.pos.actil&ACTIL_PRGED) dpllayer( 1,dpl.actimgi); else dplswloop(); break;
	case 'G': dplgimp(); break;
	case '/': dplinputtxtinit(ITM_SEARCH); break;
	case 'h': effsw(ESW_HELP,-1); break;
	default: break;
	}
	if(key>='0' && key<='9'){
		dplinputtxtinit(ITM_NUM);
		dplinputtxtadd(key);
	}
}

char dplevdelay(struct ev *ev){
	unsigned int evi;
	Uint32 now=SDL_GetTicks();
	Uint32 nxt[DEG_NUM];
	int i;
	memset(nxt,0,sizeof(Uint32)*DEG_NUM);
	for(evi=1;ev->ev>=evi;evi<<=1) if(ev->ev&evi){
		enum dplevgrp grp=DEG_NONE;
		Uint32 nxttime=1000;
		switch(evi){
		case DE_RIGHT:   if(ev->src==DES_KEY && !dplwritemode()) grp=DEG_RIGHT; break;
		case DE_LEFT:    if(ev->src==DES_KEY && !dplwritemode()) grp=DEG_LEFT;  break;
		case DE_ZOOMIN:  grp=DEG_ZOOMIN;  if(dpl.pos.zoom!=-1) nxttime=0; break;
		case DE_ZOOMOUT: grp=DEG_ZOOMOUT; if(dpl.pos.zoom!=(panoactive()?2:1)) nxttime=0; break;
		case DE_PLAY:
		case DE_STOP:    if(ev->src==DES_KEY) grp=DEG_PLAY; break;
		case DE_DIR:     grp=DEG_PLAY; nxttime=500; break;
		}
		if(grp==DEG_NONE) continue;
		if(dpl.evdelay[grp]>now) return 0;
		if(!nxttime) continue;
		if(nxt[grp]<nxttime) nxt[grp]=nxttime;
	}
	for(i=0;i<DEG_NUM;i++) if(nxt[i]) dpl.evdelay[i]=now+nxt[i];
	return 1;
}

char dplev(struct ev *ev){
	char ret=0;
	int clickimg;
	unsigned int evi;
	char evdone=0;
	if(!dplevdelay(ev)) return 0;
	clickimg=dplclickimg(ev->sx,ev->sy,ev->imgi);
	dpl.pos.imgiold=AIMGI;
	if(!(ev->ev&(DE_KEY|DE_STAT))) dpl.colmode=COL_NONE;
	if( (ev->ev&DE_KEY) && ev->key!='+' && ev->key!='-') dpl.colmode=COL_NONE;
	if(!(ev->ev&(DE_STAT|DE_COL|DE_KEY))) dplprgcolreset();
	if( (ev->ev&DE_KEY) && ev->key!='c' && ev->key!='C') dplprgcolreset();
	if(!(ev->ev&DE_STAT)) dplmousehold(-1);
	for(evi=1;!evdone && ev->ev>=evi;evi<<=1) if((evdone=(ev->ev&evi)!=0) && (ret=1)) switch(evi){
	case DE_MOVE:
	case DE_RIGHT:
	case DE_LEFT:
	case DE_UP:
	case DE_DOWN:
	case DE_ZOOMIN:
	case DE_ZOOMOUT: dplmove(ev->ev,ev->sx,ev->sy,clickimg); break;
	case DE_SEL:     dplsel(clickimg); break;
	case DE_DIR:     evdone=dpldir(clickimg,ev->src!=DES_MOUSE); break;
	case DE_MARK:    if((evdone=dplwritemode())) dplmark(clickimg); break;
	case DE_STOP:    if(dpl.run) dpl.run=0; else evdone=0; break;
	case DE_PLAY:
		if(!(dpl.pos.actil&ACTIL_PRGED) && !panoev(PE_PLAY) && dpl.pos.zoom<=0){
			dplmove(DE_RIGHT,0.f,0.f,-1);
			dpl.run=dplgetticks();
		}
	break;
	case DE_KEY: dplkey(ev->key); break;
	case DE_STAT:
		if(!dplactil(ev->sx,clickimg)) ret=0;
		dplmousehold(clickimg);
	break;
	case DE_JUMPEND:
		if(!dplwritemode() || !mapcltsave(clickimg-IMGI_MAP)) dplprged("imgpos",1,dpl.actimgi,-1);
	break;
	case DE_COL: effprgcolset(clickimg); break;
	case DE_INFO:
		if(clickimg>=0 && clickimg<32){
			if(dpl.infosel&(1U<<clickimg)) dpl.infosel&=~(1U<<clickimg);
			else dpl.infosel|=1U<<clickimg;
		}
	break;
	case DE_MAPMK:
		dplinputtxtinitp(ITM_MAPMK,ev->sx,ev->sy);
	break;
	}
	if(AIMGI==IMGI_END && !dpl.cfg.playmode) dpl.run=0;
	if(dplwritemode() || dpl.pos.zoom!=0 || ev->ev!=DE_RIGHT || AIMGI==IMGI_END) ret|=2;
	return ret;
}

void dplcheckev(){
	char statchg=0;
	int mid;
	for(mid=0;mid<DPLEV_NMOVE;mid++) if(dev.move[mid].sx || dev.move[mid].sy){
		enum dplev ev=0;
		if(dev.move[mid].sx) ev|=DE_JUMPX;
		if(dev.move[mid].sy) ev|=DE_JUMPY;
		dpljump(mid,dev.move[mid].sx,dev.move[mid].sy,dev.move[mid].imgi,ev);
		dev.move[mid].sx=0.f;
		dev.move[mid].sy=0.f;
	}
	while(dev.wi!=dev.ri){
		statchg|=dplev(dev.evs+dev.ri);
		dev.ri=(dev.ri+1)%DPLEVS_NUM;
	}
	if(dpl.resortil){
		ilsort(-1,dpl.resortil,ILSCHG_NONE);
		dpl.resortil=NULL;
		statchg|=1;
	}
	if(statchg&1) dplstatupdate();
	if(statchg&2 && !dpl.cfg.playrecord) effstaton(statchg);
}

/***************************** dpl run ****************************************/

void dplrun(){
	Uint32 time=dplgetticks();
	unsigned int dpldur = AIMGI==IMGI_END ? 2000 : dpl.cfg.displayduration;
	if(time-dpl.run>effdelay(imginarrorlimits(0,AIMGI),dpldur)){
		if(dpl.cfg.playmode && AIMGI==IMGI_END) sdl_quit=1;
		dpl.run=time;
		dplevput(DE_RIGHT);
	}
}

/***************************** dpl thread *************************************/

void dplcfginit(){
	int z;
	const char *t;
	memset(dev.move,0,sizeof(dev.move));
	dpl.cfg.displayduration=cfggetuint("dpl.displayduration");
	dpl.cfg.holdduration=cfggetuint("dpl.holdduration");
	dpl.cfg.loop=cfggetbool("dpl.loop");
	dpl.cfg.prged_w=cfggetfloat("prged.w");
	dpl.cfg.playmode=cfggetbool("dpl.playmode");
	t=cfggetstr("sdpl.playrecord");
	dpl.cfg.playrecord=t && t[0];
	dpl.cfg.playrecord_rate=cfggetuint("dpl.playrecord_rate");
	dpl.infosel=cfggetuint("dpl.infosel");
	z=cfggetint("dpl.initzoom");
	for(;z>0;z--) dplevput(DE_ZOOMOUT);
	for(;z<0;z++) dplevput(DE_ZOOMIN);
	memset(dpl.evdelay,0,sizeof(Uint32)*DEG_NUM);
	if(dpl.cfg.playrecord) dpl.cfg.playmode=1;
	if(dpl.cfg.playmode) dpl.run=dplgetticks()-dpl.cfg.displayduration+1000;
}

int dplthread(void *UNUSED(arg)){
	Uint32 last=0;
	dplcfginit();
	effcfginit();
	dplstatupdate();
	if(!dpl.cfg.playrecord) effstaton();
	effinit(EFFREF_CLR,DE_INIT,-1);
	while(!sdl_quit){

		if(dpl.run) dplrun();
		else dpl.cfg.playmode=0;
		panorun();
		timer(TI_DPL,0,0);
		dplcheckev();
		dplmouseholdchk();
		timer(TI_DPL,1,0);
		effdo();
		timer(TI_DPL,2,0);
		sdldelay(&last,16);
		timer(TI_DPL,3,0);
	}
	dplprgcolreset();
	sdl_quit|=THR_DPL;
	return 0;
}
