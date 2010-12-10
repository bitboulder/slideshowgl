#include <stdlib.h>
#include <SDL.h>
#include <math.h>
#include "dpl.h"
#include "sdl.h"
#include "load.h"
#include "cfg.h"
#include "main.h"
#include "exif.h"
#include "act.h"
#include "file.h"
#include "eff.h"

#define IMGI_START	INT_MIN
#define IMGI_END	INT_MAX

enum colmode { COL_NONE=-1, COL_G=0, COL_C=1, COL_B=2 };

char *colmodestr[]={"G","B","C"};

struct dpl {
	struct dplpos pos;
	Uint32 run;
	char showinfo;
	char showhelp;
	int inputnum;
	struct {
		Uint32 displayduration;
		char loop;
	} cfg;
	enum colmode colmode;
} dpl = {
	.pos.imgi = IMGI_START,
	.pos.zoom = 0,
	.pos.x = 0.,
	.pos.y = 0.,
	.pos.writemode = 0,
	.run = 0,
	.showinfo = 0,
	.showhelp = 0,
	.inputnum = 0,
	.colmode = COL_NONE,
};

/***************************** dpl interface **********************************/

/* thread: all */
struct dplpos *dplgetpos(){ return &dpl.pos; }
int dplgetimgi(){
	if(dpl.pos.imgi==IMGI_START) return -1;
	if(dpl.pos.imgi==IMGI_END) return nimg;
	return dpl.pos.imgi;
}
int dplgetzoom(){ return dpl.pos.zoom; }
char dplshowinfo(){ return dpl.showinfo; }
int dplinputnum(){ return dpl.inputnum; }
char dplloop(){ return dpl.cfg.loop; }

/***************************** imgfit *****************************************/

char imgfit(struct img *img,float *fitw,float *fith){
	float irat;
	float srat=sdlrat();
	enum rot rot;
	if(!img || !(irat=imgldrat(img->ld))) return 0;
	rot=imgexifrot(img->exif);
	if(rot==ROT_90 || rot==ROT_270) irat=1./irat;
	*fitw = srat>irat ? irat/srat : 1.;
	*fith = srat>irat ? 1. : srat/irat;
	return 1;
}

char imgspos2ipos(struct img *img,float sx,float sy,float *ix,float *iy){
	float fitw,fith;
	if(dpl.pos.zoom<0) return 0;
	if(panospos2ipos(img,sx,sy,ix,iy)) return 1;
	if(!imgfit(img,&fitw,&fith)) return 0;
	fitw*=powf(2.f,(float)dpl.pos.zoom);
	fith*=powf(2.f,(float)dpl.pos.zoom);
	*ix = sx/fitw;
	*iy = sy/fith;
	return 1;
}

void printixy(float sx,float sy){
	struct img *img=imgget(dpl.pos.imgi);
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
	{ .move=5, .size=1.,    .inc=0,  .texcur=TEX_BIG,   .texoth=TEX_BIG,   },
	{ .move=3, .size=1./3., .inc=4,  .texcur=TEX_SMALL, .texoth=TEX_SMALL, },
	{ .move=5, .size=1./5., .inc=12, .texcur=TEX_SMALL, .texoth=TEX_TINY,  },
	{ .move=7, .size=1./7., .inc=24, .texcur=TEX_TINY,  .texoth=TEX_TINY,  },
};

void dplclippos(struct img *img){
	float xb,yb;
	float x[2],y[2];
	char clipx;
	if(!imgspos2ipos(img,.5f,.5f,&xb,&yb)) return;
	clipx=panoclipx(img,&xb);
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

void dplmovepos(float sx,float sy){
	float ix,iy;
	struct img *img=imgget(dpl.pos.imgi);
	if(!img || !imgspos2ipos(img,sx,sy,&ix,&iy)) return;
	dpl.pos.x+=ix;
	dpl.pos.y+=iy;
	dplclippos(img);
	debug(DBG_DBG,"dpl move pos => imgi %i zoom %i pos %.2fx%.2f",dpl.pos.imgi,dpl.pos.zoom,dpl.pos.x,dpl.pos.y);
}

void dplzoompos(int nzoom,float sx,float sy){
	float oix,oiy,nix,niy;
	char o,n;
	struct img *img=imgget(dpl.pos.imgi);
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
	int i = imgi ? *imgi : dpl.pos.imgi;
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
	if(imgi) *imgi=i; else dpl.pos.imgi=i;
}

void dplchgimgi(int dir){
	if(dpl.pos.imgi<0)     dpl.pos.imgi=-1;
	if(dpl.pos.imgi>=nimg) dpl.pos.imgi=nimg;
	dpl.pos.imgi+=dir;
}

void dplmove(enum dplev ev,float x,float y){
	const static int zoommin=sizeof(zoomtab)/sizeof(struct zoomtab);
	int dir=DE_DIR(ev);
	dpl.pos.imgiold=dpl.pos.imgi;
	switch(ev){
	case DE_RIGHT:
	case DE_LEFT:
		if(!panoev(dir<0?PE_SPEEDLEFT:PE_SPEEDRIGHT)){
			if(dpl.pos.zoom<=0) dplchgimgi(dir);
			else dplmovepos((float)dir*.25f,0.f);
		}
	break;
	case DE_UP:
	case DE_DOWN:
		if(dpl.pos.zoom<0)  dplchgimgi(-dir*zoomtab[-dpl.pos.zoom].move);
		if(dpl.pos.zoom==0) dplchgimgi( dir*zoomtab[-dpl.pos.zoom].move);
		else dplmovepos(0.f,-(float)dir*.25f);
	break;
	case DE_ZOOMIN:
	case DE_ZOOMOUT:
	{
		float x;
		if(dpl.pos.zoom==0 && dir>0 && panostart(&x)){
			struct img *img=imgget(dpl.pos.imgi);
			dpl.pos.x=x;
			dpl.pos.zoom+=dir;
			dplclippos(img);
		}else if(dpl.pos.zoom+dir<=0){
			if(dpl.pos.zoom==1) effpanoend(dpl.pos.imgi);
			dpl.pos.x=dpl.pos.y=0.;
			dpl.pos.zoom+=dir;
		}else dplzoompos(dpl.pos.zoom+dir,x,y);
	}
	break;
	default: return;
	}
	if(dpl.pos.zoom<1-zoommin) dpl.pos.zoom=1-zoommin;
	if(dpl.pos.zoom>0)    dpl.run=0;
	dplclipimgi(NULL);
	if((dpl.pos.imgi<0 || dpl.pos.imgi>=nimg) && dpl.pos.zoom>0) dpl.pos.zoom=0;
	debug(DBG_STA,"dpl move => imgi %i zoom %i pos %.2fx%.2f",dpl.pos.imgi,dpl.pos.zoom,dpl.pos.x,dpl.pos.y);
	effinit(EFFREF_ALL|EFFREF_FIT,ev,-1);
}

int dplclickimg(float sx,float sy){
	int i,x,y;
	if(dpl.pos.imgi<0)     return 0;
	if(dpl.pos.imgi>=nimg) return nimg-1;
	if(dpl.pos.zoom>=0)    return dpl.pos.imgi;
	sx/=effmaxfit().w; if(sx> .49f) sx= .49f; if(sx<-.49f) sx=-.49f;
	sy/=effmaxfit().h; if(sy> .49f) sy= .49f; if(sy<-.49f) sy=-.49f;
	x=floor(sx/zoomtab[-dpl.pos.zoom].size+.5f);
	y=floor(sy/zoomtab[-dpl.pos.zoom].size+.5f);
	i=floor((float)y/zoomtab[-dpl.pos.zoom].size+.5f);
	i+=x;
	return dpl.pos.imgi+i;
}

void dplsel(int imgi){
	dpl.pos.imgi=imgi;
	dplclipimgi(NULL);
	effinit(EFFREF_ALL|EFFREF_FIT,DE_SEL,-1);
}

void dplmark(int imgi){
	struct img *img;
	char *mark;
	if(!dpl.pos.writemode) return;
	dplclipimgi(&imgi);
	if(!(img=imgget(imgi))) return;
	mark=imgposmark(img->pos);
	*mark=!*mark;
	effinit(EFFREF_IMG,DE_MARK,imgi);
	actadd(ACT_SAVEMARKS,NULL);
}

void dplrotate(enum dplev ev){
	struct img *img=imgget(dpl.pos.imgi);
	int r=DE_DIR(ev);
	if(!img) return;
	exifrotate(img->exif,r);
	effinit(EFFREF_IMG|EFFREF_FIT,ev,-1);
	if(dpl.pos.writemode) actadd(ACT_ROTATE,img);
}

void dpldel(){
	struct img *img=imgdel(dpl.pos.imgi);
	if(!img) return;
	if(dpl.pos.zoom>0) dpl.pos.zoom=0;
	if(delimg){
		struct img *tmp=delimg;
		delimg=NULL;
		ldffree(tmp->ld,TEX_NONE);
	}
	effdel(img->pos);
	delimg=img;
	if(dpl.pos.writemode) actadd(ACT_DELETE,img);
}

void dplcol(int d){
	struct img *img;
	float *val;
	if(dpl.colmode==COL_NONE) return;
	if(!(img=imgget(dpl.pos.imgi))) return;
	val=((float*)imgposcol(img->pos))+dpl.colmode;
	*val+=.1f*(float)d;
	if(*val<-1.f) *val=-1.f;
	if(*val> 1.f) *val= 1.f;
}

#define ADDTXT(...)	txt+=snprintf(txt,dsttxt+ISTAT_TXTSIZE-txt,__VA_ARGS__)
void dplstatupdate(){
	char *dsttxt=effstat()->txt;
	if(dpl.pos.imgi<0) snprintf(dsttxt,ISTAT_TXTSIZE,_("BEGIN"));
	else if(dpl.pos.imgi>=nimg) snprintf(dsttxt,ISTAT_TXTSIZE,_("END"));
	else{
		struct img *img=imgs[dpl.pos.imgi];
		struct icol *ic=imgposcol(img->pos);
		char *txt=dsttxt;
		ADDTXT("%i/%i %s",dpl.pos.imgi+1, nimg, imgfilefn(img->file));
		switch(imgexifrot(img->exif)){
			case ROT_0: break;
			case ROT_90:  ADDTXT(_(" rotated-right")); break;
			case ROT_180: ADDTXT(_(" rotated-twice")); break;
			case ROT_270: ADDTXT(_(" rotated-left")); break;
		}
		if(dpl.pos.writemode){
			ADDTXT(_(" (write-mode)"));
			if(*imgposmark(img->pos)) ADDTXT(_(" [MARK]"));
		}
		if(dpl.colmode!=COL_NONE || ic->g || ic->c || ic->b){
			ADDTXT(" %sG:%.1f%s",dpl.colmode==COL_G?"[":"",ic->g,dpl.colmode==COL_G?"]":"");
			ADDTXT(" %sC:%.1f%s",dpl.colmode==COL_C?"[":"",ic->c,dpl.colmode==COL_C?"]":"");
			ADDTXT(" %sB:%.1f%s",dpl.colmode==COL_B?"[":"",ic->b,dpl.colmode==COL_B?"]":"");
		}
	}
}

/***************************** dpl action *************************************/

void dplsetdisplayduration(int dur){
	if(dur<100) dur*=1000;
	if(dur>=250 && dur<=30000) dpl.cfg.displayduration=dur;
}

/***************************** dpl key ****************************************/

#define DPLEVS_NUM	128
struct dplevs {
	struct ev {
		enum dplev ev;
		SDLKey key;
		float sx,sy;
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
void dplevputx(enum dplev ev,SDLKey key,float sx,float sy){
	if(ev==DE_MOVE){
		dev.move.sy+=sy;
		dev.move.sx+=sx;
	}else{
		int nwi=(dev.wi+1)%DPLEVS_NUM;
		dev.evs[dev.wi].ev=ev;
		dev.evs[dev.wi].key=key;
		dev.evs[dev.wi].sx=sx;
		dev.evs[dev.wi].sy=sy;
		if(nwi!=dev.ri) dev.wi=nwi;
	}
}

char *keyboardlayout=
	__("Mouse left drag")"\0"     __("Move")"\0"
	__("Mouse left button")"\0"   __("Goto image / Forward")"\0"
	__("Mouse middle button")"\0" __("Toggle mark (only in writing mode)")"\0"
	__("Mouse right button")"\0"  __("Backward")"\0"
	__("Mouse scroll")"\0"        __("Zoom in/out")"\0"
	__("Space")"\0"               __("Stop/Play")"\0"
	__("Right")"\0"               __("Forward (Zoom: shift right)")"\0"
	__("Left")"\0"                __("Backward (Zoom: shift left)")"\0"
	__("Up")"\0"                  __("Fast forward (Zoom: shift up)")"\0"
	__("Down")"\0"                __("Fast backward (Zoom: shift down)")"\0"
	__("Pageup")"\0"              __("Zoom in")"\0"
	__("Pagedown")"\0"            __("Zoom out")"\0"
	__("[0-9]+Enter")"\0"         __("Goto image with number")"\0"
	__("[0-9]+d")"\0"             __("Displayduration [s/ms]")"\0"
	__("f")"\0"                   __("Switch fullscreen")"\0"
	__("r/R")"\0"                 __("Rotate image")"\0"
	__("w")"\0"                   __("Switch writing mode")"\0"
	__("g/b/c")"\0"               __("+/- mode: gamma/brightness/contrase")"\0"
	__("+/-")"\0"                 __("Increase/decrease selected")"\0"
	__("Del")"\0"                 __("Move image to del/ and remove from dpl-list (only in writing mode)")"\0"
	__("m")"\0"                   __("Toggle mark (only in writing mode)")"\0"
	__("i")"\0"                   __("Show image info")"\0"
	__("h")"\0"                   __("Show help")"\0"
	__("q/Esc")"\0"               __("Quit")"\0"
	"\0"
;

char *dplhelp(){
	return dpl.showhelp ? keyboardlayout : NULL;
}

void dplkey(SDLKey key){
	debug(DBG_STA,"dpl key %i",key);
	switch(key){
	case SDLK_ESCAPE:   if(dpl.inputnum || dpl.showinfo || dpl.showhelp) break;
	case SDLK_q:        sdl_quit=1; break;
	case SDLK_BACKSPACE:panoev(PE_PLAIN); break;
	case SDLK_f:        sdlfullscreen(); break;
	case SDLK_w:        dpl.pos.writemode=!dpl.pos.writemode; effrefresh(EFFREF_ALL); break;
	case SDLK_m:        dplmark(dpl.pos.imgi); break;
	case SDLK_d:        dplsetdisplayduration(dpl.inputnum); break;
	case SDLK_g:        dpl.colmode=COL_G; break;
	case SDLK_c:        dpl.colmode=COL_C; break;
	case SDLK_b:        dpl.colmode=COL_B; break;
	case SDLK_RETURN:   dplsel(dpl.inputnum-1); break;
	case SDLK_DELETE:   if(dpl.pos.writemode) dpldel(); break;
	case SDLK_PLUS:     dplcol(1); break;
	case SDLK_MINUS:    dplcol(-1); break;
	default: break;
	}
	if(key>=SDLK_0 && key<=SDLK_9){
		dpl.inputnum = dpl.inputnum*10 + (key-SDLK_0);
	}else dpl.inputnum=0;
	dpl.showinfo = !dpl.showinfo && key==SDLK_i &&
		dpl.pos.imgi>=0 && dpl.pos.imgi<nimg;
	dpl.showhelp = !dpl.showhelp && key==SDLK_h;
}

char dplev(struct ev *ev){
	char ret=1;
	if(ev->ev!=DE_KEY && ev->ev!=DE_STAT) dpl.colmode=COL_NONE;
	if(ev->ev==DE_KEY && ev->key!=SDLK_PLUS && ev->key!=SDLK_MINUS) dpl.colmode=COL_NONE;
	switch(ev->ev){
	case DE_RIGHT:
	case DE_LEFT:
	case DE_UP:
	case DE_DOWN:
	case DE_ZOOMIN:
	case DE_ZOOMOUT: dplmove(ev->ev,ev->sx,ev->sy); break;
	case DE_SEL:  dplsel(dplclickimg(ev->sx,ev->sy)); break;
	case DE_MARK: dplmark(dplclickimg(ev->sx,ev->sy)); break;
	case DE_ROT1: 
	case DE_ROT2: dplrotate(ev->ev); break;
	case DE_PLAY: 
		if(!panoev(PE_PLAY) && dpl.pos.zoom<=0)
			dpl.run=dpl.run ? 0 : -100000;
	break;
	case DE_KEY: dplkey(ev->key); break;
	default: ret=0; break;
	}
	if(dpl.pos.writemode || (dpl.pos.zoom<=0 && ev->ev!=DE_RIGHT) || dpl.pos.imgi>=nimg) ret|=2;
	return ret;
}

void dplcheckev(){
	char stat=0;
	while(dev.wi!=dev.ri){
		stat|=dplev(dev.evs+dev.ri);
		dev.ri=(dev.ri+1)%DPLEVS_NUM;
	}
	if(dev.move.sx){
		dplmovepos(dev.move.sx,dev.move.sy);
		dev.move.sx=0.f;
		dev.move.sy=0.f;
		effinit(EFFREF_IMG,DE_MOVE,-1);
	}
	if(stat&1) dplstatupdate(stat);
	if(stat&2) effstaton(stat);
}

/***************************** dpl run ****************************************/

void dplrun(){
	Uint32 time=SDL_GetTicks();
	if(time-dpl.run>dpl.cfg.displayduration){
		dpl.run=time;
		dplevput(DE_RIGHT);
	}
}

/***************************** dpl thread *************************************/

void dplcfginit(){
	dpl.cfg.displayduration=cfggetint("dpl.displayduration");
	dpl.cfg.loop=cfggetint("dpl.loop");
}

int dplthread(void *arg){
	Uint32 last=0;
	dplcfginit();
	effcfginit();
	dplstatupdate();
	effstaton();
	while(!sdl_quit){

		dplcheckev();
		if(dpl.run) dplrun();
		timer(TI_DPL,0,0);
		effdo();
		timer(TI_DPL,1,0);
		panorun();
		timer(TI_DPL,2,0);

		sdlthreadcheck();
		timer(TI_DPL,3,0);
		sdldelay(&last,16);
		timer(TI_DPL,4,0);
	}
	sdl_quit|=THR_DPL;
	return 0;
}
