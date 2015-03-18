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
#include "hist.h"

#define INPUTTXTBACK	0x80
#define INPUTTXTTAB 	0x81

enum colmode { COL_NONE=-1, COL_G=0, COL_C=1, COL_B=2 };

enum dplevgrp { DEG_RIGHT, DEG_LEFT, DEG_ZOOMIN, DEG_ZOOMOUT, DEG_PLAY, DEG_NUM, DEG_NONE };

enum inputtxt {ITM_OFF, ITM_CATSEL, ITM_TXTIMG, ITM_NUM, ITM_SEARCH, ITM_MAPSCH, ITM_DIRED, ITM_MAPMK, ITM_MARKFN};

const char *colmodestr[]={"G","B","C"};

struct dplrun {
	Uint32 time;
	enum {DRM_OFF,DRM_STD,DRM_RET,DRM_PLD,DRM_PDO,DRM_MOV} mode;
	char pit,prot;
};

struct dpl {
	struct dplpos pos;
	char writemode;
	struct dplrun run;
	struct {
		Uint32 displayduration;
		Uint32 holdduration;
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
	enum diredmode {DEM_FROM,DEM_TO,DEM_ALL} diredmode;
	struct dplmousehold {
		int imgi;
		Uint32 time;
	} mousehold;
	char fullscreen;
	char *lastmark;
	Uint32 catforce;
	char cmdfin;
} dpl = {
	.pos.zoom = 0,
	.pos.x = 0.,
	.pos.y = 0.,
	.pos.buble = NULL,
	.writemode = 0,
	.run.mode = DRM_OFF,
	.colmode = COL_NONE,
	.input.mode = ITM_OFF,
	.fid = 0,
	.mousehold.time = 0,
	.fullscreen = 0,
	.lastmark = NULL,
	.catforce = 0,
};

#define AIL_ON		(dpl.pos.ailtyp!=AIL_NONE)
#define AIL_LEFT	(AIL_ON && cilgetacti()==0)
#define AIL_RIGHT	(AIL_ON && cilgetacti()==1)

/***************************** dpl interface **********************************/

/* thread: all */
struct dplpos *dplgetpos(){ return &dpl.pos; }
int dplgetzoom(){ return dpl.pos.zoom; }
float dplgetz(){ return dpl.pos.zoom<0?0.f:powf(2.f,.5f*(float)dpl.pos.zoom); }

struct dplinput *dplgetinput(){
	if(dpl.input.mode==ITM_OFF) return NULL;
	if(dpl.input.in[0]=='\0'){
		static struct dplinput in={ .in={'\0'}, .post={'\0'}, .id=-1 };
		switch(dpl.input.mode){
		case ITM_CATSEL: snprintf(in.pre,FILELEN,_("[Catalog]")); break;
		case ITM_TXTIMG: snprintf(in.pre,FILELEN,_("[Text]")); break;
		case ITM_NUM:    snprintf(in.pre,FILELEN,_("[Number]")); break;
		case ITM_MAPSCH:
		case ITM_SEARCH: snprintf(in.pre,FILELEN,_("[Search]")); break;
		case ITM_DIRED:
		case ITM_MAPMK:  snprintf(in.pre,FILELEN,_("[Directory]")); break;
		case ITM_MARKFN: snprintf(in.pre,FILELEN,_("[Mark-File]")); break;
		}
		return &in;
	}
	return &dpl.input;
}
int dplgetactil(char *prged){
	if(!AIL_ON) return -1;
	if(prged) *prged=dpl.pos.ailtyp==AIL_PRGED;
	return cilgetacti();
}

/* thread: sdl */
unsigned int dplgetfid(){ return dpl.fid++; }

char dplwritemode(){
	if(dpl.writemode) return dpl.writemode;
	return (dpl.pos.ailtyp&AIL_ED)!=0;
}

Uint32 dplgetticks(){
	if(dpl.cfg.playrecord) return dpl.fid*1000/dpl.cfg.playrecord_rate;
	return SDL_GetTicks();
}

char *dplgetinfo(){
	struct img *img;
	if(!(img=ilcimg(NULL))) return NULL;
	return imgexifinfo(img->exif);
}

float *dplgethist(){
	struct img *img;
	if(!(img=ilcimg(NULL))) return NULL;
	if(imgfiledir(img->file)) return NULL;
	return imghistget(img->hist);
}

enum catforce { CF_ONRUN, CF_ON, CF_RUN, CF_RUNC, CF_OFF, CF_CHK };
void catforce(enum catforce cf){
	switch(cf){
	case CF_ONRUN:
	case CF_ON:  if(effsw(ESW_CAT,1) || dpl.catforce) dpl.catforce=1; if(cf!=CF_ONRUN) break;
	case CF_RUNC:
	case CF_RUN: if(dpl.catforce) dpl.catforce=SDL_GetTicks()+(cf==CF_RUNC?0:2000); break;
	case CF_OFF: dpl.catforce=0; break;
	case CF_CHK: if(dpl.catforce>1 && SDL_GetTicks()>=dpl.catforce){ dpl.catforce=0; effsw(ESW_CAT,0); } break;
	}
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
	float z=dplgetz();
	if(!z) return 0;
	if(panospos2ipos(img,sx,sy,ix,iy)) return 1;
	if(!imgfit(img,&fitw,&fith)) return 0;
	fitw*=z;
	fith*=z;
	if(ix) *ix = sx/fitw;
	if(iy) *iy = sy/fith;
	return 1;
}

void printixy(float sx,float sy){
	struct img *img=ilcimg(NULL);
	float ix,iy;
	if(!img || !(imgspos2ipos(img,sx,sy,&ix,&iy))) return;
	debug(DBG_NONE,"img pos %.3fx%.3f",ix+dpl.pos.x,iy+dpl.pos.y);
}

/***************************** dpl run ****************************************/

char dplrun(char dst){
	char chg=0;
	struct img *img;
	if(dpl.run.mode!=DRM_OFF && dst==0){
		dpl.run.mode=DRM_OFF;
		return 1;
	}
	switch(dpl.run.mode){
	case DRM_OFF:
		if(dst>0){
			chg=1;
			dpl.run.mode=DRM_STD;
			dplevput(DE_RIGHT);
			dpl.run.time=dplgetticks();
			if(dst==2) dpl.run.time+=1000-dpl.cfg.displayduration;
		}
	break;
	case DRM_STD:
	case DRM_RET:
		if(dpl.pos.zoom>0 || (ilcimgi(NULL)==IMGI_END && !dpl.cfg.playmode)){
			chg=1;
			dpl.run.mode=DRM_OFF;
		}else{
			Uint32 time=dplgetticks();
			unsigned int dpldur=dpl.cfg.displayduration;
			char pano=dpl.run.mode!=DRM_RET && (img=ilcimg(NULL)) && imgpanoenable(img->pano) && dpl.pos.zoom==0;
			char mov =dpl.run.mode!=DRM_RET && (img=ilcimg(NULL)) && imgfilemov(img->file) && dpl.pos.zoom==0;
			if(ilcimgi(NULL)==IMGI_END) dpldur=2000;
			if(pano || mov || dpl.run.mode==DRM_RET) dpldur=1500;
			if(time-dpl.run.time>effdelay(ilcimgi(NULL),dpldur)){
				if(dpl.cfg.playmode && ilcimgi(NULL)==IMGI_END) sdl_quit=1;
				if(pano){
					dpl.run.mode=DRM_PLD;
					dplevput(DE_ZOOMIN);
				}else if(mov){
					dpl.run.mode=DRM_MOV;
					dpl.cmdfin=0;
					dplevput(DE_MOV);
				}else{
					dpl.run.mode=DRM_STD;
					dpl.run.time=time;
					dplevput(DE_RIGHT);
				}
			}
		}
	break;
	case DRM_PLD:
		if((img=ilcimg(NULL)) && imgldtex(img->ld,TEX_PANO)){
			dpl.run.mode=DRM_PDO;
			dpl.run.time=dplgetticks();
			dpl.run.pit=dpl.run.prot=0;
			panoev(PE_PLAY);
		}
	break;
	case DRM_PDO: {
		char prot=panorot()<0.f ? -1 : 1;
		Uint32 dur=dplgetticks()-dpl.run.time;
		if(dpl.run.prot!=prot && dur>1000){
			dpl.run.prot=prot;
			if(dur>30000) dpl.run.pit++;
			if(++dpl.run.pit>2){
				dplevput(DE_ZOOMOUT);
				dpl.run.mode=DRM_RET;
				dpl.run.time=dplgetticks();
			}
		}
	break; }
	case DRM_MOV:
		if(dpl.cmdfin){
			dpl.run.mode=DRM_RET;
			dpl.run.time=dplgetticks();
		}
	break;
	}
	if(dpl.run.mode==DRM_OFF) dpl.cfg.playmode=0;
	return chg;
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
	{ .move=9, .size=1.f/9.f, .inc=40, .texcur=TEX_TINY,  .texoth=TEX_TINY,  },
	{ .move=11,.size=1.f/11.f,.inc=60, .texcur=TEX_TINY,  .texoth=TEX_TINY,  },
	{ .move=13,.size=1.f/13.f,.inc=84, .texcur=TEX_TINY,  .texoth=TEX_TINY,  },
	{ .move=15,.size=1.f/15.f,.inc=112,.texcur=TEX_TINY,  .texoth=TEX_TINY,  },
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
	if(dpl.pos.ailtyp==AIL_PRGED){
		struct ecur *ecur;
		if(!AIL_RIGHT || !(img=ilimg(CIL(1),dpl.actimgi))) return 0;
		sx/=1.f-dpl.cfg.prged_w;
		ecur=imgposcur(img->pos);
		ecur->x-=sx;
		ecur->y-=sy;
		return 0;
	}else if(mapon()){
		return mapmovepos(sx,sy);
	}else{
		float ix,iy;
		if(!(img=ilcimg(NULL)) || !imgspos2ipos(img,sx,sy,&ix,&iy)) return 0;
		panotrimmovepos(img,&ix,&iy);
		dpl.pos.x+=ix;
		dpl.pos.y+=iy;
		dplclippos(img);
		debug(DBG_DBG,"dpl move pos => imgi %i zoom %i pos %.2fx%.2f",ilcimgi(NULL),dpl.pos.zoom,dpl.pos.x,dpl.pos.y);
		return 1;
	}
}

void dplzoompos(int nzoom,float sx,float sy){
	float oix,oiy,nix,niy;
	char o,n;
	struct img *img=ilcimg(NULL);
	o=imgspos2ipos(img,sx,sy,&oix,&oiy);
	dpl.pos.zoom=nzoom;
	n=imgspos2ipos(img,sx,sy,&nix,&niy);
	if(o && n){
		dpl.pos.x+=oix-nix;
		dpl.pos.y+=oiy-niy;
	}
	if(img) dplclippos(img);
}

void dplsecdir(){
	struct imglist *il;
	struct img *img;
	const char *dir;
	if(dpl.pos.ailtyp!=AIL_DIRED) return;
	if(!(img=ilcimg(CIL(0)))) return;
	if(!(dir=imgfiledir(img->file))) return;
	if(!(il=floaddir(imgfilefn(img->file),dir))) return;
	if(cilset(il,1,1)==1) return;
	ilupdcimgi(il);
	effinit(EFFREF_CLR,0,CIL(1),-1);
}

void dplchgimgi(int dir){
	ilsetcimgi(NULL,ilrelimgi(NULL,ilcimgi(NULL))+dir);
}

int dplclickimg(float sx,float sy,int evimgi){
	int i,x,y;
	int imgi=ilcimgi(NULL);
	if(evimgi>=-1)        return evimgi;
	if(mapon())           return -1;
	if(imgi==IMGI_START)  return IMGI_START;
	if(imgi==IMGI_END)    return IMGI_END;
	if(dpl.pos.zoom>=0)   return imgi;
	sx/=effmaxfit().w; if(sx> .49f) sx= .49f; if(sx<-.49f) sx=-.49f;
	sy/=effmaxfit().h; if(sy> .49f) sy= .49f; if(sy<-.49f) sy=-.49f;
	x=(int)floorf(sx/zoomtab[-dpl.pos.zoom].size+.5f);
	y=(int)floorf(sy/zoomtab[-dpl.pos.zoom].size+.5f);
	i=(int)floorf((float)y/zoomtab[-dpl.pos.zoom].size+.5f);
	i+=x;
	return imgi+i;
}

char dplprged(const char *cmd,int il,int imgi,int arg){
	struct img *img=NULL;
	struct prg *prg;
	char buf[FILELEN*2];
	if(dpl.pos.ailtyp!=AIL_PRGED) return 0;
	if(il>=0 && cilgetacti()!=il) return 0;
	if(il<0) il=-il-1;
	if(!(prg=ilprg(CIL(1)))) return 0;
	if(imgi>=0 && !(img=ilimg(CIL(il),imgi))) return 0;
	if(img && imgfiledir(img->file)) return 0;
	if(cmd){
		const char *fn="";
		struct txtimg *txt=NULL;
		if(img) fn=imgfilefn(img->file);
		if(!strcmp(cmd,"txtadd")) fn=dpl.input.in;
		if(img && (txt=imgfiletxt(img->file))) fn=txt->txt;
		snprintf(buf,FILELEN*2,"%s \"%s\" %i",cmd,fn,ilcimgi(CIL(1)));
		if(img && !strcmp(cmd,"imgpos")){
			size_t len=strlen(buf);
			struct ecur *ecur=imgposcur(img->pos);
			snprintf(buf+len,FILELEN*2-len," :%.3f:%.3f:%.3f",
				ecur->s,ecur->x,ecur->y);
		}
		if(!strcmp(cmd,"frmmov") || !strcmp(cmd,"frmcpy")){
			size_t len=strlen(buf);
			if(arg==-1) arg=ilcimgi(CIL(1))+1;
			else if(arg==-2) arg=ilcimgi(CIL(1))-1;
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
	if(!ilreload(CIL(1),cmd?buf:NULL)) return 1;
	effprgcolinit(NULL,-2);
	effinit(EFFREF_ALL,strcmp(cmd,"reload")?DE_JUMP:0,CIL(1),-1);
	return 1;
}

void dpllayer(char dir,int imgi){
	struct img *img=ilimg(CIL(1),imgi);
	if(!img) return;
	if(dir>0) imgposopt(img->pos)->layer++;
	else      imgposopt(img->pos)->layer--;
	dplprged("imgon",-2,imgi,-1);
}

void dplprgcolreset(){
	int saveimgi;
	if(dpl.pos.ailtyp!=AIL_PRGED) return;
	saveimgi=effprgcolinit(NULL,-1);
	if(saveimgi>=0) dplprged("imgcol",1,saveimgi,-1);
}

char dplprgcol(){
	struct img *img;
	struct txtimg *txt=NULL;
	int saveimgi;
	int initimgi=dpl.actimgi;
	if(dpl.pos.ailtyp!=AIL_PRGED) return 0;
	if(!AIL_RIGHT || !(img=ilimg(CIL(1),dpl.actimgi)) ||
			!(txt=imgfiletxt(img->file)))
		initimgi=-1;
	saveimgi=effprgcolinit(txt?txt->col:NULL,initimgi);
	if(saveimgi>=0) dplprged("imgcol",1,saveimgi,-1);
	return 1;
}

char dplprgcolcopy(){
	int imgi=effprgcolinit(NULL,-1);
	struct img *img;
	if(dpl.pos.ailtyp!=AIL_PRGED) return 0;
	if(imgi<0)  imgi=dpl.actimgi;
	if(AIL_RIGHT && (img=ilimg(CIL(1),imgi)) && imgfiletxt(img->file))
		dplprged("frmcol", 1,imgi,-1);
	return 1;
}

void dplmove(enum dplev ev,float sx,float sy,int clickimg){
	static const int zoommin=sizeof(zoomtab)/sizeof(struct zoomtab);
	int dir=DE_DIR(ev);
	if(ev&DE_MOVE) dplmovepos(sx,sy);
	if(mapmove(ev,sx,sy)) goto end;
	if(ev&(DE_RIGHT|DE_LEFT)){
		if(!panoev(dir<0?PE_SPEEDLEFT:PE_SPEEDRIGHT)){
			if(dpl.pos.zoom<=0) dplchgimgi(dir);
			else dplmovepos((float)dir*.25f,0.f);
		}
	}
	if(ev&(DE_UP|DE_DOWN)){
		if(AIL_ON && AIL_LEFT) dplchgimgi(-dir);
		else if(dpl.pos.zoom<0)  dplchgimgi(-dir*zoomtab[-dpl.pos.zoom].move);
		else if(dpl.pos.zoom==0) dplchgimgi( dir*zoomtab[-dpl.pos.zoom].move);
		else dplmovepos(0.f,-(float)dir*.25f);
	}
	if(ev&(DE_ZOOMIN|DE_ZOOMOUT)){
		float x,fitw;
		struct img *img;
		if(dpl.pos.ailtyp==AIL_PRGED){
			if((img=ilimg(CIL(1),dpl.actimgi))){
				imgposcur(img->pos)->s *= dir>0 ? sqrtf(2.f) : sqrtf(.5f);
				dplprged("imgpos",1,dpl.actimgi,-1);
			}
			return;
		}
		if((dpl.pos.ailtyp==AIL_DIRED) && AIL_LEFT) return;
		if((dpl.pos.ailtyp==AIL_DIRED) && dir>0 && dpl.pos.zoom>=-1) return;
		if(dpl.pos.zoom<0 && clickimg>=0) ilsetcimgi(NULL,clickimg);
		img=ilcimg(NULL);
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
	ilupdcimgi(NULL);
	if((ilcimgi(NULL)==IMGI_START || ilcimgi(NULL)==IMGI_END) && dpl.pos.zoom>0) dpl.pos.zoom=0;
	debug(DBG_STA,"dpl move => imgi %i zoom %i pos %.2fx%.2f",ilcimgi(NULL),dpl.pos.zoom,dpl.pos.x,dpl.pos.y);
end:
	effinit(EFFREF_ALL|EFFREF_FIT,ev,NULL,-1);
	dplsecdir();
}

void dplsel(int imgi){
	if(imgi<0 || imgi==IMGI_START || imgi==IMGI_END) return;
	ilsetcimgi(NULL,imgi);
	effinit(EFFREF_ALL|EFFREF_FIT,DE_SEL,NULL,-1);
	dplsecdir();
}

char dplmark(int imgi,char copy){
	struct img *img;
	char *mark;
	size_t mkid=0;
	if(!dplwritemode() || mapon()) return 0;
	if(imgi==IMGI_START || imgi==IMGI_END) return 0;
	if(imgi>=IMGI_CAT){
		mkid=(size_t)imgi-IMGI_CAT+1;
		imgi=ilcimgi(NULL);
		if(imgi==IMGI_START || imgi==IMGI_END) return 0;
		if(mkid>markncat()) return 0;
	}
	imgi=ilclipimgi(NULL,imgi,0);
	if(!(img=ilimg(NULL,imgi))) return 0;
	if(imgfiledir(img->file)) return 0;
	if(copy && !dpl.lastmark) return 0;
	mark=imgposmark(img,MPC_ALLWAYS);
	if(!copy){
		mark[mkid]=!mark[mkid];
		markchange(mkid);
	}else for(mkid=1;mkid<=markncat();mkid++) if(dpl.lastmark[mkid]!=mark[mkid]){
		mark[mkid]=dpl.lastmark[mkid];
		markchange(mkid);
	}
	dpl.lastmark=mark;
	effinit(EFFREF_IMG,DE_MARK,NULL,imgi);
	return 1;
}

void dplrotate(enum dplev ev){
	struct img *img=ilcimg(NULL);
	int r=DE_DIR(ev);
	if(!img) return;
	if(imgfiledir(img->file)) return;
	exifrotate(img->exif,r);
	effinit(EFFREF_IMG|EFFREF_FIT,ev,NULL,-1);
	if(dplwritemode()) actadd(ACT_ROTATE,img,NULL);
}

enum edpldel { DD_DEL, DD_ORI };
void dpldel(enum edpldel act){
	struct img *img=NULL;
	if(!dplwritemode()) return;
	if(act==DD_ORI && (img=ilcimg(NULL)) && fimgswitchmod(img)){
		effinit(EFFREF_IMG|EFFREF_FIT,0,NULL,-1);
	}else if((img=ildelcimg(NULL))){
		markimgclean(img);
		if(dpl.pos.zoom>0) dpl.pos.zoom=0;
		dpl.pos.imgiold--;
		effdel(img);
		effinit(EFFREF_ALL|EFFREF_FIT,DE_RIGHT,NULL,-1);
	}else return;
	actadd(act==DD_ORI ? ACT_DELORI : ACT_DELETE,img,ilget(NULL));
}

char dplcol(int d){
	struct img *img;
	float *val;
	if(dpl.colmode==COL_NONE) return 0;
	if(!(img=ilcimg(NULL))) return 0;
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
	if(AIL_ON && !AIL_LEFT) return 0;
	if(imgi>=IMGI_CAT && imgi<IMGI_END && !(fn=markcatfn(imgi-IMGI_CAT,&dir))) return 1;
	if(imgi>=IMGI_MAP && imgi<IMGI_CAT){
		if(!mapgetclt(imgi-IMGI_MAP,&il,&fn,&dir)) return 1;
		dpl.pos.zoom=-2; /* TODO: ?? */
	}
	if(imgi==IMGI_START) return 0;
	if(!il && !fn && !(img=ilimg(NULL,imgi))){
		if(noleave) return 0;
		il=ilget(NULL);
		if(!cilset(NULL,0,0)) return 1;
		if(dpl.pos.ailtyp==AIL_DIRED && !ildired(NULL)) cilset(il,0,0);
	}else{
		if(!il){
			if(!fn){
				if(!(dir=imgfiledir(img->file))) return 0;
				fn=imgfilefn(img->file);
				ilsetcimgi(NULL,imgi);
			}else actforce();
			if(!(il=floaddir(fn,dir))) return 1;
			if(dpl.pos.ailtyp==AIL_DIRED && !ildired(il)) return 1;
		}
		if(cilset(il,0,0)==1) return 1;
		if(ilprg(CIL(0))){ dpl.pos.zoom=0; ilsetcimgi(NULL,IMGI_START); }
		else if(ilcimgi(NULL)==IMGI_START) ilsetcimgi(NULL,0);
	}
	dplrun(0);
	dpl.pos.imgiold=imgi;
	effinit(EFFREF_CLR,0,NULL,-1);
	sdlforceredraw(); /* TODO remove (for switch to map which does not use eff) */
	dplsecdir();
	return 1;
}

#define ADDTXT(...)	{ snprintf(txt,(size_t)(dsttxt+ISTAT_TXTSIZE-txt),__VA_ARGS__); txt+=strlen(txt); }
void dplstatupdate(){
	char *dsttxt=effstat()->txt;
	char run=dpl.run.mode!=DRM_OFF;
	int imgi=ilcimgi(NULL);
	struct img *img;
	if(mapstatupdate(dsttxt)) goto end;
	if(imgi==IMGI_START) snprintf(dsttxt,ISTAT_TXTSIZE,_("BEGIN"));
	else if(imgi==IMGI_END) snprintf(dsttxt,ISTAT_TXTSIZE,_("END"));
	else if(!(img=ilcimg(NULL))) return;
	else{
		struct icol *ic;
		char *txt=dsttxt;
		const char *tmp;
		ic=imgposcol(img->pos);
		ADDTXT("%i/%i ",imgi+1,ilnimgs(NULL));
		if(!ilprg(NULL) || dpl.pos.zoom!=0){
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
		if((tmp=ilsortget(NULL))) ADDTXT(" sort:%s",tmp);
		if(dpl.colmode!=COL_NONE || ic->g || ic->c || ic->b){
			ADDTXT(" %sG:%.1f%s",dpl.colmode==COL_G?"[":"",ic->g,dpl.colmode==COL_G?"]":"");
			ADDTXT(" %sC:%.1f%s",dpl.colmode==COL_C?"[":"",ic->c,dpl.colmode==COL_C?"]":"");
			ADDTXT(" %sB:%.1f%s",dpl.colmode==COL_B?"[":"",ic->b,dpl.colmode==COL_B?"]":"");
		}
		if(dpl.pos.zoom>0){
			float sw,iw,fitw;
			sdlwh(&sw,NULL);
			if(imgldwh(img->ld,&iw,NULL) && imgfit(img,&fitw,NULL))
				ADDTXT(" (%.0f%%)",sw/iw*fitw*dplgetz()*100.f);
		}
		run|=panostattxt(txt,(size_t)(dsttxt-ISTAT_TXTSIZE-txt));
	}
	effstat()->run=run;
end:
	sdlforceredraw();
}

void dplinputtxtfinal(char ok);

char dplactil(float x,int clickimg){
	int ail;
	if(!AIL_ON) return 0;
	ail = x+.5f>dpl.cfg.prged_w;
	if(ail==cilgetacti()) return 0;
	if(dpl.input.mode==ITM_SEARCH) dplinputtxtfinal(1);
	cilsetact(ail);
	if(dpl.pos.ailtyp==AIL_PRGED) dpl.actimgi=clickimg;
	sdlforceredraw();
	return 1;
}

enum cmdact {CA_NONE=0x00, CA_FS=0x01, CA_BUBLE=0x02, CA_FIN=0x04};
int dplcmdrun(void *arg){
	char *cmd=arg;
	enum cmdact ca=cmd[0];
	system(cmd+1);
	free(arg);
	if(ca&CA_FS) dpl.fullscreen=1;
	if(ca&CA_BUBLE) dpl.pos.buble=NULL;
	if(ca&CA_FIN) dpl.cmdfin=1;
	return 0;
}

void dplextprg(const char *prg,char raw){
	struct img *img=ilcimg(NULL);
	char *cmd;
	const char *fn;
	if(!img) return;
	if(imgfiledir(img->file)) return;
	if(!(fn=raw?imgfileraw(img->file):imgfilefn(img->file))) return;
	sdlfullscreen(0);
	cmd=malloc(FILELEN*8);
	snprintf(cmd,FILELEN+8,"%c%s %s",CA_NONE,prg,fn);
	SDL_CreateThread(dplcmdrun,"dplextprg",cmd);
}

char dplmov(){
	struct img *img=ilcimg(NULL);
	char *cmd;
	enum cmdact ca=CA_FIN;
	const char *mov;
	if(!img) return 0;
	if(!(mov=imgfilemov(img->file))) return 0;
	if(mov[0]=='m' && sdlfullscreen(0)) ca|=CA_FS;
	if(mov[0]=='w'){ dpl.pos.buble=img; ca|=CA_BUBLE; effinit(EFFREF_IMG,0,NULL,ilcimgi(NULL)); }
	dpl.cmdfin=0;
	cmd=malloc(FILELEN*2);
	snprintf(cmd,FILELEN*2,"%csleep 0.5; mpv %s\"%s\" >/dev/null",ca,(ca&CA_FS)?"-fs ":"",mov+1);
	SDL_CreateThread(dplcmdrun,"dplmov",cmd);
	return 1;
}

void dplconvert(){
	struct img *img=ilcimg(NULL);
	char *cmd;
	if(!img) return;
	if(filetype(imgfilefn(img->file))&(FT_DIREX|FT_FILE)) return;
	sdlfullscreen(0);
	cmd=malloc(FILELEN*8);
	snprintf(cmd,FILELEN+8,"%ccnv_ui -img %s",CA_NONE,imgfilefn(img->file));
	SDL_CreateThread(dplcmdrun,"dplconvert",cmd);
}

void dplswloop(){ ilcfgswloop(); effinit(EFFREF_ALL,0,NULL,-1); }

void dpledit(){
	enum cilsecswitch type;
	if(AIL_ON){
		if(dpl.pos.ailtyp==AIL_PRGED) type=ILSS_PRGOFF;
		else if(dpl.pos.ailtyp==AIL_DIRED) type=ILSS_DIROFF;
		else return;
	}else{
		if(ilprg(NULL)) type=ILSS_PRGON;
		else if(ildired(NULL)) type=ILSS_DIRON;
		else return;
	}
	if(!cilsecswitch(type)) return;
	dplrun(0);
	switch(type){
	case ILSS_PRGOFF:
	case ILSS_DIROFF:
		dpl.pos.ailtyp=AIL_NONE;
		cilsetact(0);
		effinit(EFFREF_ALL,0,NULL,-1);
	break;
	case ILSS_PRGON:
		dpl.actimgi=-1;
		dpl.pos.zoom=0;
		dpl.pos.ailtyp=AIL_PRGED;
		effinit(EFFREF_CLR,0,CIL(0),-1);
		effinit(EFFREF_CLR,0,CIL(1),-1);
	break;
	case ILSS_DIRON:
		dpl.pos.ailtyp=AIL_DIRED;
		dpl.pos.zoom=-2;
		ilupdcimgi(NULL);
		effinit(EFFREF_CLR,0,NULL,-1);
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
	size_t i;
	if(!(img=ilcimg(CIL(0)))) return;
	srcdir=imgfilefn(img->file);
//	if(!dir(srcdir)) return;
	if(dpl.diredmode==DEM_FROM && ilcimgi(CIL(1))<=0) dpl.diredmode=DEM_ALL;
	if(dpl.diredmode==DEM_TO   && ilcimgi(CIL(1))>=ilnimgs(CIL(1))-1) dpl.diredmode=DEM_ALL;
	if(id<0){
		updatedirs=1;
		if(!(dstdir=ilfn(CIL(0)))) return;
		snprintf(dstbuf,FILELEN,"%s/%s",dstdir,input);
		dstdir=dstbuf;
		//if(-e dstdir) return;
	}else{
		if(!(img=ilimg(CIL(0),id))) return;
		dstdir=imgfilefn(img->file);
		// if(!dir(dstdir)) return;
	}
	dsttdir[0]='\0';
	for(i=strlen(srcdir);i--;) if((srcdir[i]=='/' || srcdir[i]=='\\') && !strncmp(srcdir,dstdir,i+1)){
		memcpy(srctdir,srcdir,i);
		snprintf(srctdir+i,FILELEN-i,"/thumb/%s",srcdir+i+1);
		if(filetype(srctdir)&FT_DIR){
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
		if(id<0 && !mkdirm(dstdir,0)) return;
		if(dsttdir[0] && !mkdirm(dsttdir,0)) return;
		img=dpl.diredmode==DEM_FROM ? ilcimg(CIL(1)) : ilimg(CIL(1),0);
		imgend=dpl.diredmode==DEM_TO ? ilcimg(CIL(1)) : NULL;
		for(;img;img=img->nxt){
			const char *fn=imgfilefn(img->file);
			frename(fn,dstdir);
			if(dsttdir[0] && imgfiletfn(img->file,&fn)) frename(fn,dsttdir);
			if(img==imgend) break;
		}
		if(dpl.diredmode==DEM_ALL){
			if(!rmdir(srcdir)) updatedirs=1;
			if(dsttdir[0]) rmdir(srctdir);
		}
	}
	if(updatedirs && (il=floaddir(ilfn(CIL(0)),ildir(CIL(0))))){
		ilsetcimgi(il,ilcimgi(CIL(0)));
		cilset(il,0,1);
		effinit(EFFREF_ALL,DE_INIT,CIL(0),-1);
	}
	dplsecdir();
}

void dplmap(){
	if(mapon()) mapswtype(); else{
		struct imglist *il=mapsetpos(ilcimg(NULL));
		if(il){
			cilset(il,-1,0);
			effinit(EFFREF_ALL,DE_ZOOM,NULL,-1);
			sdlforceredraw();
		}
	}
}

void dpljump(int mid,float sx,float sy,int imgi,enum dplev ev){
	switch(mid){
	case 0: if(dplmovepos(sx,sy)) effinit(EFFREF_IMG,ev,NULL,-1); break;
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
		uint32_t key;
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
void dplevputx(enum dplev ev,uint32_t key,float sx,float sy,int imgi,enum dplevsrc src){
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
#define IF_MAP
#include "dpl_help.h"
#undef  IF_MAP
#define IF_MAPED
#include "dpl_help.h"
#undef  IF_MAPED
#define IF_PRGED
#include "dpl_help.h"
#undef  IF_PRGED
#define IF_DIRED
#include "dpl_help.h"
#undef  IF_DIRED

/* thread: gl */
const char *dplhelp(){
	if(dpl.pos.ailtyp==AIL_PRGED) return dlphelp_prged;
	if(dpl.pos.ailtyp==AIL_DIRED) return dlphelp_dired;
	if(mapon() && dpl.writemode)  return dlphelp_maped;
	if(mapon())                   return dlphelp_map;
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

void dplimgsearch(struct dplinput *in){
	int nimgs=ilnimgs(in->il);
	int i = in->srcid<0 || in->srcid>=nimgs ? 0 : (in->srcid+1)%nimgs;
	int n=0;
	size_t ilen=strlen(in->in);
	if(in->in[0]) for(;n<nimgs;n++,i=(i+1)%nimgs){
		struct img *img=ilimg(in->il,i);
		const char *fn, *pos;
		size_t plen,p;
		if(!img || !(fn=imgfilefn(img->file))) continue;
		pos=strrchr(fn,'/');
		if(pos) pos++; else pos=fn;
		if((plen=strlen(pos))<ilen) continue;
		for(p=0;p<=plen-ilen;p++){
			if(strncasecmp(pos+p,in->in,ilen)) continue;
			memcpy(in->pre,pos,p);
			in->pre[p]='\0';
			memcpy(in->in,pos+p,ilen);
			memcpy(in->post,pos+p+ilen,plen-p-ilen);
			in->post[plen-p-ilen]='\0';
			if(in->mode==ITM_SEARCH) dplsel(i);
			else in->id = !in->pre[0] && !in->post[0] ? i : -1;
			return;
		}
	}
	in->pre[0]='\0';
	in->post[0]='\0';
	if(in->mode==ITM_SEARCH) dplsel(in->srcid);
	else in->id=-1;
}

void dplfilesearch(struct dplinput *in){
	const char *bdir;
	int id;
	size_t len=strlen(in->in);
	char onlydir;
	switch(dpl.input.mode){
	case ITM_MAPMK:  bdir=mapgetbasedirs(); onlydir=1; break;
	case ITM_MARKFN: bdir=markgetfndir();   onlydir=0; break;
	default: return;
	}
	in->id=-1;
	in->pre[0]='\0';
	in->post[0]='\0';
	in->res[0]='\0';
	if(bdir) for(id=0;bdir[0];bdir+=FILELEN*2,id++){
		const char *bname=bdir+FILELEN;
		size_t l=strlen(bname);
		if(!l){
			finddirmatch(in->in,in->post,in->res,bdir,onlydir);
			if(in->post[0]) break;
		}else if(len<=l && !strncmp(bname,in->in,len) && (l==len || strncmp(in->post,bname+len,l-len)<0)){
			snprintf(in->post,MAX(FILELEN,l-len),bname+len);
			snprintf(in->res,FILELEN,bdir);
		}else if(len>l && !strncmp(bname,in->in,l) && (in->in[l]=='/' || in->in[l]=='\\')){
			finddirmatch(in->in+l+1,in->post,in->res,bdir,onlydir);
			if(in->post[0]) break;
		}
	}
	if(!in->res[0] && dpl.input.mode==ITM_MARKFN) snprintf(in->res,FILELEN,"%s/%s",markgetfndir(),in->in);
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
		}else if(dpl.input.mode==ITM_MAPMK || dpl.input.mode==ITM_MARKFN)
			if(len<FILELEN-1) dpl.input.in[len++]='/';
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
	case ITM_SEARCH: dplimgsearch(&dpl.input); break;
	case ITM_MAPSCH: mapsearch(&dpl.input); break;
	case ITM_DIRED:  dplimgsearch(&dpl.input); break;
	case ITM_MAPMK:
	case ITM_MARKFN: dplfilesearch(&dpl.input); break;
	}
	sdlforceredraw();
}

#define dplinputtxtinit(mode)	dplinputtxtinitp(mode,0.f,0.f)
char dplinputtxtinitp(enum inputtxt mode,float x,float y){
	if(mode==ITM_SEARCH && ilprg(NULL)) return 0;
	if(mode==ITM_DIRED  && dpl.pos.ailtyp!=AIL_DIRED) return 0;
	if(mode==ITM_MAPMK  && (!dpl.writemode || !mapon())) return 0;
	if(mode==ITM_MAPSCH) mapsavepos();
	if(mode==ITM_MAPSCH || mode==ITM_MAPMK) mapinfo(-1);
	if(mode==ITM_MARKFN && ((dpl.pos.ailtyp&AIL_ED) || !dpl.writemode)) return 0;
	dpl.input.pre[0]='\0';
	dpl.input.in[0]='\0';
	dpl.input.post[0]='\0';
	dpl.input.id=-1;
	dpl.input.x=x;
	dpl.input.y=y;
	dpl.input.mode=mode;
	if(mode==ITM_SEARCH || mode==ITM_DIRED){
		dpl.input.il = mode==ITM_DIRED ? CIL(0) : NULL;
		dpl.input.srcid=ilcimgi(dpl.input.il);
	}
	sdlforceredraw();
	return 1;
}

void dplinputtxtfinal(char ok){
	size_t len=strlen(dpl.input.in);
	switch(dpl.input.mode){
	case ITM_CATSEL:
		if(ok && dpl.input.id>=0) dplevputi(DE_MARK,dpl.input.id+IMGI_CAT);
		catforce(ok?CF_RUN:CF_RUNC);
	break;
	case ITM_TXTIMG: if(ok && len) dplprged("txtadd",-1,-1,-1); break;
	case ITM_NUM:    if(ok && len) dplsel(atoi(dpl.input.in)-1); break;
	case ITM_SEARCH: if(!ok) dplsel(dpl.input.id); break;
	case ITM_MAPSCH: if(!ok) maprestorepos(); break;
	case ITM_DIRED:  if(ok) dpldired(dpl.input.in,dpl.input.id); break;
	case ITM_MAPMK:
		if(ok && dpl.input.res[0]) mapmarkpos(dpl.input.x,dpl.input.y,dpl.input.res);
	break;
	case ITM_MARKFN:
	printf("%i %lu +%s+%s+\n",ok,len,dpl.input.res,dpl.input.in);
		if(ok && len && markswitchfn(dpl.input.res)) ileffref(CIL_ALL,EFFREF_ALL); break;
	}
	dpl.input.lastmode=dpl.input.mode;
	dpl.input.mode=ITM_OFF;
	sdlforceredraw();
}

void dplsearchnext(){
	struct dplinput in=dpl.input;
	if(in.lastmode!=ITM_SEARCH) return;
	in.mode=in.lastmode;
	in.srcid=ilcimgi(in.il);
	dplimgsearch(&in);
}

void dplkey(uint32_t key){
	int inputnum=-1;
	if(!key) return;
	debug(DBG_STA,"dpl key 0x%08x",key);
	if(dpl.input.mode!=ITM_OFF){
		if(key<0x20 || key==0x7f) switch(key){
			case '\b': dplinputtxtadd(INPUTTXTBACK); break;
			case '\t': dplinputtxtadd(INPUTTXTTAB); break;
			case '\n': dplinputtxtfinal(1); break;
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
	case ' ': dplevput(DE_STOP|DE_DIR|DE_MOV|DE_PLAY);       break;
	case  27: if(effsw(ESW_CAT,0)) break;
			  if(effsw(ESW_INFO,0)) break;
			  if(effsw(ESW_HELP,0)) break;
	case 'q': sdl_quit=1; break;
	case  '\b': if(!panoev(PE_MODE)) dplevputi(DE_DIR,IMGI_END); break;
	case 'r': dplrotate(DE_ROT1); break;
	case 'R': dplrotate(DE_ROT2); break;
	case 'p': panoev(PE_FISHMODE); break;
	case 'f': sdlfullscreen(-1); break;
	case 'w':
		if(dpl.pos.ailtyp&AIL_ED) cilsetact(!cilgetacti()); else{
			dpl.writemode = dpl.writemode==1 ? 0 : 1;
			ileffref(CIL_ALL,EFFREF_ALL);
		}
	break;
	case 'W':
		if(!(dpl.pos.ailtyp&AIL_ED)){
			dpl.writemode = dpl.writemode==2 ? 0 : 2;
			ileffref(CIL_ALL,EFFREF_ALL);
		}
	break;
	case 'm':
		if(!dplprged("frmmov",1,-1,inputnum)){
			if(dplinputtxtinit(ITM_DIRED)) dpl.diredmode=DEM_FROM;
			else if(mapon() || !dplmark(ilcimgi(NULL),0)) dplmap();
		}
	break;
	case 'M':
		if(!dplprged("frmmov",1,-1,-2)){
			if(dplinputtxtinit(ITM_DIRED)) dpl.diredmode=DEM_TO;
			else dplinputtxtinit(ITM_MARKFN);
		}
	break;
	case 'j': if(dplinputtxtinit(ITM_DIRED)) dpl.diredmode=DEM_ALL; break;
	case 'd': if(!mapdisplaymode() && !dplprged("frmcpy",1,-1,inputnum) && inputnum>=0) dplsetdisplayduration(inputnum); break;
	case 'g': if(glprg()) dpl.colmode=COL_G; break;
	case 'c': if(!dplprgcol() && glprg()) dpl.colmode=COL_C; break;
	case 'C': if(!dplprgcolcopy()) dplconvert(); break;
	case 'b': if(glprg()) dpl.colmode=COL_B; break;
	case 'k': effsw(ESW_CAT,-1); catforce(CF_OFF); break;
	case 's': if(dplwritemode() && dplinputtxtinit(ITM_CATSEL)) catforce(CF_ON); break;
	case 'S': if(dplmark(ilcimgi(NULL),1)) catforce(CF_ONRUN); else{ ilsortchg(NULL); effinit(EFFREF_ALL,0,NULL,-1); } break;
	case 127: if(dplwritemode()) dpldel(DD_DEL); break;
	case 'o': if(dplwritemode()) dpldel(DD_ORI); break;
	case '+': if(!dplprged("imgadd",-1,AIL_LEFT && dpl.actimgi>=0 ? dpl.actimgi : ilcimgi(CIL(0)),-1)) dplcol(1); break;
	case '-': if(!dplprged("imgdel", 1,dpl.actimgi,-1)) dplcol(-1); break;
	case 'e':
		if(!mapon()) dpledit();
		else if(dplwritemode()) mapeditmode();
	break;
	case 'E': if(!dplprged("reload",-1,-1,-1) && ilreload(CIL(1),NULL)) effinit(EFFREF_ALL,0,CIL(1),-1); break;
	case 'i':
		if(dpl.pos.ailtyp==AIL_PRGED) dplprged("frmins",-1,-1,-1);
		else if(dplgetinfo(NULL)) effsw(ESW_INFOSEL,-1);
	break;
	case 'I': if(dplgetinfo(NULL)) effsw(ESW_INFO,-1); break;
	case 'H': if(!mapelevation() && dplgethist()) effsw(ESW_HIST,-1); break;
	case 'x': if(!dplprged("frmdel",-1,-1,-1)) optx++; break;
	case 'X': optx=0; break;
	case 't': if(dpl.pos.ailtyp==AIL_PRGED) dplinputtxtinit(ITM_TXTIMG); break;
	case 'l': if(dpl.pos.ailtyp==AIL_PRGED) dpllayer(-1,dpl.actimgi); else dplswloop(); break;
	case 'L': if(dpl.pos.ailtyp==AIL_PRGED) dpllayer( 1,dpl.actimgi); else dplswloop(); break;
	case 'G': dplextprg("gimp",0); break;
	case 'U': dplextprg("ufraw",1); break;
	case '/': dplinputtxtinit(mapon()?ITM_MAPSCH:ITM_SEARCH); break;
	case 'n': dplsearchnext(); break;
	case 'h': effsw(ESW_HELP,-1); break;
	case 'D': glprgsw(); break;
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
		case DE_MOV:
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
	dpl.pos.imgiold=ilcimgi(NULL); /* TODO: move in il */
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
	case DE_MARK:    if((evdone=dplwritemode())) dplmark(clickimg,0); break;
	case DE_MOV:     evdone=dplmov(); break;
	case DE_STOP:    if(!dplrun(0)) evdone=0; break;
	case DE_PLAY:    if(dpl.pos.ailtyp!=AIL_PRGED && !panoev(PE_PLAY) && dpl.pos.zoom<=0) dplrun(1); break;
	case DE_KEY: dplkey(ev->key); break;
	case DE_STAT:
		if(!dplactil(ev->sx,clickimg)) ret=0;
		dplmousehold(clickimg);
	break;
	case DE_JUMPEND:
		if(!dplwritemode() || !mapcltsave(clickimg-IMGI_MAP)) dplprged("imgpos",1,dpl.actimgi,-1);
	break;
	case DE_COL: effprgcolset(clickimg); break;
	case DE_MAPMK:
		dplinputtxtinitp(ITM_MAPMK,ev->sx,ev->sy);
	break;
	}
	if(dplwritemode() || dpl.pos.zoom!=0 || ev->ev!=DE_RIGHT || ilcimgi(NULL)==IMGI_END) ret|=2;
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
	if(statchg&1) dplstatupdate();
	if(statchg&2 && !dpl.cfg.playrecord) effstaton(statchg);
}

/***************************** dpl thread *************************************/

void dplcfginit(){
	int z;
	const char *t;
	memset(dev.move,0,sizeof(dev.move));
	dpl.cfg.displayduration=cfggetuint("dpl.displayduration");
	dpl.cfg.holdduration=cfggetuint("dpl.holdduration");
	dpl.cfg.prged_w=cfggetfloat("prged.w");
	dpl.cfg.playmode=cfggetbool("dpl.playmode");
	t=cfggetstr("sdpl.playrecord");
	dpl.cfg.playrecord=t && t[0];
	dpl.cfg.playrecord_rate=cfggetuint("dpl.playrecord_rate");
	z=cfggetint("dpl.initzoom");
	for(;z>0;z--) dplevput(DE_ZOOMOUT);
	for(;z<0;z++) dplevput(DE_ZOOMIN);
	memset(dpl.evdelay,0,sizeof(Uint32)*DEG_NUM);
	if(dpl.cfg.playrecord) dpl.cfg.playmode=1;
	if(dpl.cfg.playmode) dplrun(2);
}

int dplthread(void *UNUSED(arg)){
	Uint32 last=0;
	dplcfginit();
	effcfginit();
	dplstatupdate();
	if(!dpl.cfg.playrecord) effstaton();
	effinit(EFFREF_CLR,DE_INIT,NULL,-1);
	while(!sdl_quit){

		if(dpl.fullscreen){ sdlfullscreen(1); dpl.fullscreen=0; }
		ilsftcheck();
		if(dpl.run.mode!=DRM_OFF) dplrun(-1);
		panorun();
		catforce(CF_CHK);
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
