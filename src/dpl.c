#include <stdlib.h>
#include <SDL.h>
#include <math.h>
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

#define INPUTTXTBACK	0x80

enum colmode { COL_NONE=-1, COL_G=0, COL_C=1, COL_B=2 };

enum dplevgrp { DEG_RIGHT, DEG_LEFT, DEG_ZOOMIN, DEG_ZOOMOUT, DEG_PLAY, DEG_NUM, DEG_NONE };

enum inputtxt {ITM_OFF, ITM_CATSEL, ITM_TXTIMG, ITM_NUM, ITM_SEARCH};

const char *colmodestr[]={"G","B","C"};

struct dpl {
	struct dplpos pos;
	Uint32 run;
	struct {
		Uint32 displayduration;
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
} dpl = {
	.pos.imgi = { IMGI_START },
	.pos.zoom = 0,
	.pos.x = 0.,
	.pos.y = 0.,
	.pos.writemode = 0,
	.run = 0,
	.colmode = COL_NONE,
	.pos.actil = 0,
	.input.mode = ITM_OFF,
	.fid = 0,
	.resortil = NULL,
};

#define AIL		(dpl.pos.actil&ACTIL)
#define AIMGI	(dpl.pos.imgi[AIL])

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
		}
		return &in;
	}
	return &dpl.input;
}
int dplgetactil(){ return (dpl.pos.actil&ACTIL_PRGED) ? AIL : -1; }
int dplgetactimgi(int il){ return (dpl.pos.actil==(ACTIL_PRGED|il)) ? dpl.actimgi : -1; }

/* thread: sdl */
unsigned int dplgetfid(){ return dpl.fid++; }

/* thread: load */
void dplsetresortil(struct imglist *il){ dpl.resortil=il; }

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

void dplchgimgi(int dir){
	AIMGI=imginarrorlimits(AIL,AIMGI)+dir;
}

int dplclickimg(float sx,float sy,int evimgi){
	int i,x,y;
	if(evimgi>=-1)         return evimgi;
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
	if(ev&DE_MOVE) dplmovepos(sx,sy);
	if(ev&(DE_RIGHT|DE_LEFT)){
		if(!panoev(dir<0?PE_SPEEDLEFT:PE_SPEEDRIGHT)){
			if(dpl.pos.zoom<=0) dplchgimgi(dir);
			else dplmovepos((float)dir*.25f,0.f);
		}
	}
	if(ev&(DE_UP|DE_DOWN)){
		if(dpl.pos.actil==ACTIL_PRGED) dplchgimgi(-dir);
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
}

void dplsel(int imgi){
	if(imgi<0 || imgi==IMGI_START || imgi==IMGI_END) return;
	AIMGI=imgi;
	dplclipimgi(NULL);
	effinit(EFFREF_ALL|EFFREF_FIT,DE_SEL,-1);
}

void dplmark(int imgi){
	struct img *img;
	char *mark;
	size_t mkid=0;
	if(!dpl.pos.writemode) return;
	if(imgi==IMGI_START || imgi==IMGI_END) return;
	if(imgi>=IMGI_CAT){
		mkid=(size_t)imgi-IMGI_CAT+1;
		imgi=AIMGI;
		if(imgi==IMGI_START || imgi==IMGI_END) return;
		if(mkid>markncat()) return;
	}
	dplclipimgi(&imgi);
	if(!(img=imgget(AIL,imgi))) return;
	if(imgfiledir(img->file)) return;
	mark=imgposmark(img,MPC_ALLWAYS);
	mark[mkid]=!mark[mkid];
	effinit(EFFREF_IMG,DE_MARK,imgi);
	markchange(mkid);
	actadd(ACT_SAVEMARKS,NULL);
}

void dplrotate(enum dplev ev){
	struct img *img=imgget(AIL,AIMGI);
	int r=DE_DIR(ev);
	if(!img) return;
	if(imgfiledir(img->file)) return;
	exifrotate(img->exif,r);
	effinit(EFFREF_IMG|EFFREF_FIT,ev,-1);
	if(dpl.pos.writemode) actadd(ACT_ROTATE,img);
}

enum edpldel { DD_DEL, DD_ORI };
void dpldel(enum edpldel act){
	struct img *img=NULL;
	if(!dpl.pos.writemode) return;
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
	if(AIL!=0) return 0;
	if(imgi>=IMGI_CAT && imgi<IMGI_END && !(fn=markcatfn(imgi-IMGI_CAT,&dir))) return 1;
	if(imgi==IMGI_START) return 0;
	if(!fn && !(img=imgget(AIL,imgi))){
		if(noleave) return 0;
		imgi=ilswitch(NULL);
		if(imgi==IMGI_END) return 1;
	}else{
		struct imglist *il;
		if(!fn){
			if(!(dir=imgfiledir(img->file))) return 0;
			fn=imgfilefn(img->file);
			AIMGI=imgi;
		}else actforce();
		if(!(il=floaddir(fn,dir))) return 1;
		imgi=ilswitch(il);
		if(imgi==IMGI_START && !ilprg(0)) imgi=0;
		if(ilprg(0)){ dpl.pos.zoom=0; imgi=IMGI_START; }
	}
	dpl.run=0;
	AIMGI=imgi;
	dpl.pos.imgiold=imgi;
	effinit(EFFREF_CLR,0,-1);
	return 1;
}

#define ADDTXT(...)	{ snprintf(txt,(size_t)(dsttxt+ISTAT_TXTSIZE-txt),__VA_ARGS__); txt+=strlen(txt); }
void dplstatupdate(){
	char *dsttxt=effstat()->txt;
	char run=dpl.run!=0;
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
		if(dpl.pos.writemode){
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
	sdlforceredraw();
}

void dplinputtxtfinal(char ok);

char dplactil(float x,int clickimg){
	int actil;
	if(!(dpl.pos.actil&ACTIL_PRGED)) return 0;
	actil = (x+.5f>dpl.cfg.prged_w) | ACTIL_PRGED;
	if(actil!=dpl.pos.actil && dpl.input.mode==ITM_SEARCH) dplinputtxtfinal(1);
	dpl.pos.actil = actil;
	dpl.actimgi   = clickimg;
	sdlforceredraw();
	return 1;
}

int dplgimprun(void *arg){
	char cmd[FILELEN+8];
	snprintf(cmd,FILELEN+8,"gimp %s",imgfilefn(((struct img *)arg)->file));
	system(cmd);
	return 0;
}

void dplgimp(){
	struct img *img=imgget(AIL,AIMGI);
	if(!img) return;
	sdlfullscreen(0);
	SDL_CreateThread(dplgimprun,img);
}

void dplswloop(){
	dpl.cfg.loop=!dpl.cfg.loop;
	effinit(EFFREF_ALL,0,-1);
}

/***************************** dpl action *************************************/

void dplsetdisplayduration(int dur){
	if(dur<100) dur*=1000;
	if(dur>=250 && dur<=30000) dpl.cfg.displayduration=(unsigned int)dur;
}

/***************************** dpl key ****************************************/

#define DPLEVS_NUM	128
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
	} move;
	int wi,ri;
} dev = {
	.wi = 0,
	.ri = 0,
	.move.sx = 0.f,
};

/* thread: sdl */
void dplevputx(enum dplev ev,unsigned short key,float sx,float sy,int imgi,enum dplevsrc src){
	if(ev&DE_JUMP){
		dev.move.sy+=sy;
		dev.move.sx+=sx;
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

const char *keyboardlayout=
__("Mouse interface")"\0"     "\0"
__("Left drag")"\0"           __("Move")"\0"
__("Left click")"\0"          __("Goto image / Forward")"\0"
__("Double click on directory")"\0" __("Enter directory")"\0"
__("Double click on space")"\0"     __("Leave directory")"\0"
__("Middle click")"\0"        __("Play/Stop / Toggle mark (only in writing mode)")"\0"
__("Right click")"\0"         __("Backward")"\0"
__("Scroll")"\0"              __("Zoom in/out")"\0"

" ""\0"                       "\0"
__("Keyboard interface")"\0"  "\0"
__("Space")"\0"               __("Stop/Play / Enter directory")"\0"
__("Back")"\0"                __("Leave directory / Toggle panorama mode (spherical,plain,fisheye)")"\0"
__("Right/Left")"\0"          __("Forward/Backward (Zoom: shift right/left)")"\0"
__("Up/Down")"\0"             __("Fast forward/backward (Zoom: shift up/down)")"\0"
__("Pageup/Pagedown")"\0"     __("Zoom in/out")"\0"
__("[0-9]+Enter")"\0"         __("Goto image with number")"\0"
__("[0-9]+d")"\0"             __("Displayduration [s/ms]")"\0"
__("f")"\0"                   __("Toggle fullscreen")"\0"
__("S")"\0"                   __("Toggle sorting of current image list")"\0"
__("l")"\0"                   __("Toggle loop of image lists")"\0"
__("r/R")"\0"                 __("Rotate image (only in writing mode permanent)")"\0"
__("w")"\0"                   __("Switch writing mode")"\0"
__("g/b/c")"\0"               __("+/- mode: gamma/brightness/contrase (only with opengl shader support)")"\0"
__("+/-")"\0"                 __("Increase/decrease selected")"\0"
__("Del")"\0"                 __("Move image to del/ and remove from dpl-list (only in writing mode)")"\0"
__("o")"\0"                   __("Move image to ori/ and remove from dpl-list (only in writing mode)")"\0"
__("m")"\0"                   __("Toggle mark (only in writing mode)")"\0"
__("i/I")"\0"                 __("Show selected/full image info")"\0"
__("k")"\0"                   __("Show image catalog")"\0"
__("s")"\0"                   __("Enter and toggle image catalog (only in writing mode)")"\0"
__("e")"\0"                   __("Open program editor")"\0"
__("G")"\0"                   __("Edit current image with gimp")"\0"
__("h")"\0"                   __("Show help")"\0"
__("p")"\0"                   __("Toggle panorama fisheye mode (isogonic,equidistant,equal-area)")"\0"
__("q/Esc")"\0"               __("Quit")"\0"
"\0"
;

const char *keyboardlayout_prged=
__("Mouse interface")"\0"     "\0"
__("Left drag")"\0"           __("Move image")"\0"
__("Left click")"\0"          __("Forward")"\0"
__("Middle click")"\0"        __("Toggle mark (only in writing mode)")"\0"
__("Right click")"\0"         __("Backward")"\0"
__("Scroll")"\0"              __("Resize image")"\0"

" ""\0"                       "\0"
__("Keyboard interface")"\0"  "\0"
__("Right/Left")"\0"          __("Forward/Backward")"\0"
__("Up/Down")"\0"             __("Fast forward/backward")"\0"
__("Pageup/Pagedown")"\0"     __("Resize image")"\0"
__("[0-9]+Enter")"\0"         __("Goto frame with number")"\0"
__("f")"\0"                   __("Toggle fullscreen")"\0"
__("r/R")"\0"                 __("Rotate image (only in writing mode permanent)")"\0"
__("w")"\0"                   __("Switch writing mode")"\0"
__("+/-")"\0"                 __("Add/remove image")"\0"
__("t")"\0"                   __("Add text")"\0"
__("Del")"\0"                 __("Move image to del/ and remove from dpl-list (only in writing mode)")"\0"
__("o")"\0"                   __("Move image to ori/ and remove from dpl-list (only in writing mode)")"\0"
__("i")"\0"                   __("Insert frame")"\0"
__("x")"\0"                   __("Delete frame")"\0"
__("m/M")"\0"                 __("Swap frame with next/previous one")"\0"
__("d")"\0"                   __("Duplicate frame")"\0"
__("[0-9]+m")"\0"             __("Move frame to position")"\0"
__("[0-9]+d")"\0"             __("Copy frame to position")"\0"
__("l/L")"\0"                 __("Move image into foreground/background")"\0"
__("c")"\0"                   __("Change color of text")"\0"
__("C")"\0"                   __("Copy color of text to all text on current frame")"\0"
__("E")"\0"                   __("Refresh current frame")"\0"
__("e")"\0"                   __("Leave program editor")"\0"
__("G")"\0"                   __("Edit current image with gimp")"\0"
__("h")"\0"                   __("Show help")"\0"
__("q/Esc")"\0"               __("Quit")"\0"
"\0"
;

/* thread: gl */
const char *dplhelp(){
	if(dpl.pos.actil&ACTIL_PRGED) return keyboardlayout_prged;
	return keyboardlayout;
}

void dplfilesearch(struct dplinput *in){
	struct img *img=imgget(AIL,0);
	int i=0;
	size_t ilen=strlen(in->in);
	if(in->in[0]) for(;img;img=img->nxt,i++){
		char *fn=imgfilefn(img->file);
		char *pos=strrchr(fn,'/');
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
			dplsel(i);
			return;
		}
	}
	in->pre[0]='\0';
	in->post[0]='\0';
	dplsel(in->id);
}

void dplinputtxtadd(uint32_t c){
	size_t len=strlen(dpl.input.in);
	if(c==INPUTTXTBACK){
		while(len && (dpl.input.in[len-1]&0xc0)==0x80) len--;
		if(len) len--;
	}else{
		char *buf=(char *)&c;
		int i;
		for(i=0;i<4 && buf[i];i++) dpl.input.in[len++]=buf[i];
	}
	dpl.input.in[len]='\0';
	switch(dpl.input.mode){
	case ITM_CATSEL: markcatsel(&dpl.input); break;
	case ITM_SEARCH: dplfilesearch(&dpl.input); break;
	}
	sdlforceredraw();
}

void dplinputtxtinit(enum inputtxt mode){
	if(mode==ITM_SEARCH && ilprg(AIL)) return;
	dpl.input.pre[0]='\0';
	dpl.input.in[0]='\0';
	dpl.input.post[0]='\0';
	dpl.input.id=-1;
	dpl.input.mode=mode;
	if(mode==ITM_SEARCH) dpl.input.id=AIMGI;
	sdlforceredraw();
}

void dplinputtxtfinal(char ok){
	size_t len=strlen(dpl.input.in);
	switch(dpl.input.mode){
	case ITM_CATSEL: if(ok && dpl.input.id>=0) dplevputi(DE_MARK,dpl.input.id+IMGI_CAT); break;
	case ITM_TXTIMG: if(ok && len) dplprged("txtadd",-1,-1,-1); break;
	case ITM_NUM:    if(ok && len) dplsel(atoi(dpl.input.in)-1); break;
	case ITM_SEARCH: if(!ok) dplsel(dpl.input.id);
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
	case 'w': dpl.pos.writemode=!dpl.pos.writemode; effrefresh(EFFREF_ALL); break;
	case 'm': if(!dplprged("frmmov",1,-1,inputnum)) dplmark(AIMGI); break;
	case 'M': dplprged("frmmov",1,-1,-2);
	case 'd': if(!dplprged("frmcpy",1,-1,inputnum) && inputnum>=0) dplsetdisplayduration(inputnum); break;
	case 'g': if(glprg()) dpl.colmode=COL_G; break;
	case 'c': if(!dplprgcol() && glprg()) dpl.colmode=COL_C; break;
	case 'C': dplprgcolcopy(); break;
	case 'b': if(glprg()) dpl.colmode=COL_B; break;
	case 'k': effsw(ESW_CAT,-1); break;
	case 's': if(dpl.pos.writemode){ dplinputtxtinit(ITM_CATSEL); effsw(ESW_CAT,1); } break;
	case 'S': ilsort(0,NULL,ILSCHG_INC); break;
	case 127: if(dpl.pos.writemode) dpldel(DD_DEL); break;
	case 'o': if(dpl.pos.writemode) dpldel(DD_ORI); break;
	case '+': if(!dplprged("imgadd",-1,!AIL && dpl.actimgi>=0 ? dpl.actimgi : dpl.pos.imgi[0],-1)) dplcol(1); break;
	case '-': if(!dplprged("imgdel", 1,dpl.actimgi,-1)) dplcol(-1); break;
	case 'e':
		if(!ilsecswitch((dpl.pos.actil&ACTIL_PRGED)!=0)) break;
		dpl.run=0;
		dpl.pos.zoom=0;
		if(dpl.pos.actil&ACTIL_PRGED){
			dpl.pos.imgi[0]=dpl.pos.imgi[1];
			dpl.pos.actil=0;
			effinit(EFFREF_ALL,0,-1);
		}else{
			dpl.pos.imgi[1]=dpl.pos.imgi[0];
			dpl.pos.imgi[0]=0;
			dpl.actimgi=-1;
			dpl.pos.actil=ACTIL_PRGED;
			effinit(EFFREF_CLR,0,-1);
			dpl.pos.actil=ACTIL_PRGED|1;
			effinit(EFFREF_ALL,0,-1);
		}
	break;
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
		case DE_RIGHT:   if(ev->src==DES_KEY && !dpl.pos.writemode) grp=DEG_RIGHT; break;
		case DE_LEFT:    if(ev->src==DES_KEY && !dpl.pos.writemode) grp=DEG_LEFT;  break;
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
	case DE_MARK:    if((evdone=dpl.pos.writemode)) dplmark(clickimg); break;
	case DE_STOP:    if(dpl.run) dpl.run=0; else evdone=0; break;
	case DE_PLAY:
		if(!(dpl.pos.actil&ACTIL_PRGED) && !panoev(PE_PLAY) && dpl.pos.zoom<=0){
			dplmove(DE_RIGHT,0.f,0.f,-1);
			dpl.run=dplgetticks();
		}
	break;
	case DE_KEY: dplkey(ev->key); break;
	case DE_STAT: if(!dplactil(ev->sx,clickimg)) ret=0; break;
	case DE_JUMPEND: dplprged("imgpos",1,dpl.actimgi,-1); break;
	case DE_COL: effprgcolset(clickimg); break;
	case DE_INFO:
		if(clickimg>=0 && clickimg<32){
			if(dpl.infosel&(1U<<clickimg)) dpl.infosel&=~(1U<<clickimg);
			else dpl.infosel|=1U<<clickimg;
		}
	break;
	}
	if(AIMGI==IMGI_END && !dpl.cfg.playmode) dpl.run=0;
	if(dpl.pos.writemode || dpl.pos.zoom!=0 || ev->ev!=DE_RIGHT || AIMGI==IMGI_END) ret|=2;
	return ret;
}

void dplcheckev(){
	char stat=0;
	if(dev.move.sx || dev.move.sy){
		enum dplev ev=0;
		if(dev.move.sx) ev|=DE_JUMPX;
		if(dev.move.sy) ev|=DE_JUMPY;
		if(dplmovepos(dev.move.sx,dev.move.sy)) effinit(EFFREF_IMG,ev,-1);
		sdlforceredraw();
		dev.move.sx=0.f;
		dev.move.sy=0.f;
	}
	while(dev.wi!=dev.ri){
		stat|=dplev(dev.evs+dev.ri);
		dev.ri=(dev.ri+1)%DPLEVS_NUM;
	}
	if(stat&1) dplstatupdate();
	if(stat&2 && !dpl.cfg.playrecord) effstaton(stat);
	if(dpl.resortil){
		ilsort(-1,dpl.resortil,ILSCHG_NONE);
		dpl.resortil=NULL;
	}
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
	dpl.cfg.displayduration=cfggetuint("dpl.displayduration");
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
