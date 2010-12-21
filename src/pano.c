#include <stdlib.h>
#include <GL/glew.h>
#include <SDL.h>
#include <SDL_image.h>
#include <math.h>
#include "pano.h"
#include "img.h"
#include "load.h"
#include "main.h"
#include "help.h"
#include "sdl.h"
#include "gl.h"
#include "cfg.h"
#include "file.h"
#include "eff.h"

#ifndef M_PI
# define M_PI		3.14159265358979323846	/* pi */
#endif
#define SIN(x)		sin((x)/180.f*M_PI)
#define COS(x)		cos((x)/180.f*M_PI)
#define TAN(x)		tan((x)/180.f*M_PI)

struct imgpano {
	char enable;
	float gw;
	float gh;
	float gyoff;
	float rotinit;
};

struct imgpano *imgpanoinit(){ return calloc(1,sizeof(struct imgpano)); }
void imgpanofree(struct imgpano *ip){ free(ip); }
char imgpanoenable(struct imgpano *ip){ return ip->enable; }

void imgpanoload(struct imgpano *ip,char *fn){
	char pfn[FILELEN];
	FILE *fd;
	ip->rotinit=4.f;
	ip->gw=ip->gh=0.f;
	strncpy(pfn,fn,FILELEN);
	if(!findfilesubdir(pfn,"ori",".pano") && !findfilesubdir(pfn,"",".pano")) goto end;
	if(!(fd=fopen(pfn,"r"))) goto end;
	fscanf(fd,"%f %f %f %f",&ip->gw,&ip->gh,&ip->gyoff,&ip->rotinit);
	fclose(fd);
end:
	if((ip->enable = ip->gw && ip->gh))
		debug(DBG_STA,"panoinit pano used: '%s'",pfn);
	else
		debug(DBG_DBG,"panoinit no pano found for '%s'",fn);
}


enum panomode { PM_NORMAL=0, PM_PLAIN=1, PM_FISHEYE=2, PM_NUM=3 };
#define E2(X,N)	FM_##X=N
enum panofm { PANOFM };
#undef E2
struct pano {
	float rot;
	char run;
	enum panomode mode;
	struct panocfg {
		float defrot;
		float minrot;
		float texdegree;
		int mintexsize;
		float radius;
		enum panofm fm;
		float maxfishangle;
	} cfg;
	GLuint pfish;
} pano;

/* thread: sdl */
void panoinit(){
	pano.cfg.defrot=cfggetfloat("pano.defrot");
	pano.cfg.minrot=cfggetfloat("pano.minrot");
	pano.cfg.texdegree=cfggetfloat("pano.texdegree");
	pano.cfg.mintexsize=cfggetint("pano.mintexsize");
	pano.cfg.radius=cfggetfloat("pano.radius");
	pano.cfg.fm=cfggetenum("pano.fishmode");
	pano.cfg.maxfishangle=cfggetfloat("pano.maxfishangle");
	pano.pfish=glprgload("vs_fish.c","fs.c");
}

/* thread: dpl */
struct img *panoactive(){
	struct img *img;
	if(dplgetzoom()<=0) return NULL;
	if(!(img=imgget(dplgetimgi()))) return NULL;
	if(!img->pano->enable) return NULL;
	return img;
}

/* thread: load */
void panores(struct img *img,struct imgpano *ip,int w,int h,int *xres,int *yres){
	while(*xres>(float)w/ip->gw*pano.cfg.texdegree) *xres>>=1;
	while(*yres>(float)h/ip->gh*pano.cfg.texdegree) *yres>>=1;
	if(!pano.pfish){
		*xres=MAX(pano.cfg.mintexsize,*xres);
		*yres=MAX(pano.cfg.mintexsize,*yres);
	}
}

/* thread: dpl, gl */
float panoclipgh(struct imgpano *ip){
	if(!pano.pfish && ip->gh>90.f) return 90.f;
	if( pano.pfish && ip->gh>=180.f) return 360.f;
	return ip->gh;
}

/* thread: dpl, gl */
#define PSPOS_DPL	-1e10
void panoperspect(struct imgpano *ip,float spos,float *perspectw,float *perspecth){
	float gh=panoclipgh(ip);
	float srat=sdlrat();
	if(spos==PSPOS_DPL) spos=powf(2.f,(float)dplgetzoom());
	if(spos<2.f) spos=powf(ip->gw/ip->gh/srat,2.f-spos);
	else spos=sqrt(2.f)/powf(spos,0.5f);
	if(perspecth) *perspecth=gh*spos;
	if(perspectw) *perspectw=gh*spos*srat;
}

/* thread: dpl */
float panoperspectrev(struct imgpano *ip,float perspectw){
	float spos=perspectw/panoclipgh(ip)/sdlrat();
	if(spos>1.f) spos=2.f-logf(spos)/logf(ip->gw/ip->gh/sdlrat());
	else spos=powf(sqrt(2.f)/spos,1.f/0.5f);
	return spos;
}


/* thread: dpl */
char panospos2ipos(struct img *img,float sx,float sy,float *ix,float *iy){
	float fitw,fith;
	float perspectw,perspecth;
	if(!img || img!=panoactive()) return 0;
	panoperspect(img->pano,PSPOS_DPL,&perspectw,&perspecth);
	fitw = img->pano->gw/perspectw;
	fith = img->pano->gh/perspecth;
	if(ix) *ix = sx/fitw;
	if(iy) *iy = sy/fith;
	return 1;
}

/* thread: dpl */
char panoclip(struct img *img,float *xb,float *yb){
	float xs,ys,ymax,yr,yp,a,c,xrotmax,xpmax,xdec;
	if(!img || img!=panoactive()) return 1;
	panoperspect(img->pano,PSPOS_DPL,&xs,&ys);
	ymax=(img->pano->gh-ys)/2.f;
	yr=abs(img->pano->gyoff)+ymax;
	yp=ys/2.f;
	a=ys/2.f/TAN(ys/2.f);
	c=yp*COS(yr)+a*SIN(yr);
	xrotmax=xs/2;
	xpmax=sqrtf((yp*yp+a*a-c*c)/(1/SIN(xrotmax)/SIN(xrotmax)-1));
	xdec=xrotmax-xpmax;
	if(xdec>=0.f) *xb+=xdec/img->pano->gw;
	if(img->pano->gh>=180.f) *yb=0.f;
	return img->pano->gw<360.f;
}

/* thread: dpl */
void panotrimmovepos(struct img *img,float *ix,float *iy){
	float perspectw,perspecth;
	if(!img || img!=panoactive()) return;
	panoperspect(img->pano,PSPOS_DPL,&perspectw,&perspecth);
	printf("%.2f %.2f\n",perspectw,perspecth);
	if(ix && perspectw>90.f) *ix/=perspectw/90.f;
	if(iy && perspecth>90.f) *iy/=perspecth/90.f;
}

/* thread: dpl */
char panostart(struct img *img,float *x){
	float perspectw;
	struct imgpano *ip;
	float fitw;
	if(!img) return 0;
	if(!img->pano->enable) return 0;
	if(!imgfit(img,&fitw,NULL)) return 0;
	ip=img->pano;
	pano.mode = ip->gh>90.f && pano.pfish ? PM_FISHEYE : PM_NORMAL;
	pano.run=0;
	pano.rot=pano.cfg.defrot;
	if(ip->rotinit<0.f) pano.rot*=-1.f;
	panoperspect(ip,1.f,&perspectw,NULL);
	*x  = ip->rotinit<0.f ? .5f : -.5f;
	*x *= ip->gw/perspectw;
	while(*x> .5f) *x-=1.f;
	while(*x<-.5f) *x+=1.f;
	imgposcur(img->pos)->s = panoperspectrev(ip,ip->gw*fitw);
	return 1;
}

/* thread: dpl */
char panoend(float *s){
	struct img *img;
	float perspecth;
	if(!(img=panoactive())) return 0;
	panoperspect(img->pano,*s,NULL,&perspecth);
	*s=img->pano->gh/perspecth;
	return 1;
}

/* thread: gl */
void panovertex(float tx,float ty){
	float xyradius,x,y,z;
	xyradius=COS(ty)*pano.cfg.radius;
	y=SIN(ty)*pano.cfg.radius;
	x=SIN(tx)*xyradius;
	z=COS(tx)*xyradius;
	glVertex3f(x,y,z);
}

/* thread: gl */
void panodrawimg(struct itx *tx,struct imgpano *ip){
	for(;tx->tex;tx++){
		glBindTexture(GL_TEXTURE_2D,tx->tex);
		glBegin(GL_QUADS);
		glTexCoord2i(0,0); panovertex((tx->x-tx->w/2.f)*ip->gw,(tx->y-tx->h/2.f)*ip->gh-ip->gyoff);
		glTexCoord2i(1,0); panovertex((tx->x+tx->w/2.f)*ip->gw,(tx->y-tx->h/2.f)*ip->gh-ip->gyoff);
		glTexCoord2i(1,1); panovertex((tx->x+tx->w/2.f)*ip->gw,(tx->y+tx->h/2.f)*ip->gh-ip->gyoff);
		glTexCoord2i(0,1); panovertex((tx->x-tx->w/2.f)*ip->gw,(tx->y+tx->h/2.f)*ip->gh-ip->gyoff);
		glEnd();
	}
}

/* thread: gl */
void panoperspective(float h3d,int fm,float w){
	GLfloat mat[16];
	int i;
	for(i=0;i<16;i++) mat[i]=0.f;
	mat[ 0]=w;
	mat[10]=fm;
	switch(fm){
	case FM_ANGLE: mat[ 5]=0.5f/tan(MIN(h3d,pano.cfg.maxfishangle)/4.f/180.f*M_PI); break;
	case FM_ADIST: mat[ 5]=1.f/(h3d/2.f/180.f*M_PI); break;
	case FM_PLANE: mat[ 5]=0.5f/sin(h3d/4.f/180.f*M_PI); break;
	case FM_ORTHO: break;
	}
	glMultMatrixf(mat);
	glUseProgram(pano.pfish);
}

/* thread: gl */
char panorender(){
	struct img *img;
	struct imgpano *ip;
	GLuint dl;
	struct ipos *ipos;
	struct icol *icol;
	enum panomode mode=pano.mode;
	float perspectw,perspecth;
	if(!(img=panoactive())) return 0;
	ip=img->pano;
	if(mode!=PM_PLAIN && !(dl=imgldtex(img->ld,TEX_PANO))) mode=PM_PLAIN;
	if(mode==PM_PLAIN && !(dl=imgldtex(img->ld,TEX_FULL))) return 0;
	ipos=imgposcur(img->pos);
	icol=imgposcol(img->pos);
	panoperspect(ip,ipos->s,&perspectw,&perspecth);
	if(mode==PM_NORMAL && perspecth>90.f && pano.pfish) mode=PM_FISHEYE;
	if(mode==PM_FISHEYE && !pano.pfish) mode=PM_NORMAL;
	glmodex(mode==PM_PLAIN?GLM_2D:GLM_3D, perspecth, mode==PM_FISHEYE?pano.cfg.fm:-1);
	glPushMatrix();
	if(glprg()) glColor4f((icol->g+1.f)/2.f,(icol->c+1.f)/2.f,(icol->b+1.f)/2.f,ipos->a);
	else glColor4f(1.f,1.f,1.f,ipos->a);
	if(mode==PM_PLAIN){
		float x=ipos->x;
		while(x<0.f) x+=1.f;
		while(x>1.f) x-=1.f;
		glScalef(ip->gw/perspectw,ip->gh/perspecth,1.f);
		glTranslatef(x,ipos->y,0.f);
		glCallList(dl);
		glTranslatef(-1.f,0.f,0.f);
		glCallList(dl);
	}else{
		glRotatef( ipos->y*ip->gh+ip->gyoff,-1.,0.,0.);
		glRotatef(-ipos->x*ip->gw, 0.,-1.,0.);
		if(mode==PM_FISHEYE){
			GLenum glerr;
			if((glerr=glGetError())){
				error(ERR_CONT,"using shader program failed (0x%04x)",glerr);
				pano.pfish=0;
			}
		}
		glCallList(dl);
	}
	glPopMatrix();
	return 1;
}

/* thread: dpl */
char panoev(enum panoev pe){
	if(!panoactive()) return 0;
	switch(pe){
	case PE_PLAY: pano.run=!pano.run; break;
	case PE_SPEEDRIGHT:
		if(!pano.run) return 0;
		if(pano.rot>0.f) pano.rot*=2.f;
		else if(pano.rot<-pano.cfg.minrot) pano.rot/=2.f;
		else pano.rot*=-1.f;
	break;
	case PE_SPEEDLEFT:
		if(!pano.run) return 0;
		if(pano.rot<0.f) pano.rot*=2.f;
		else if(pano.rot>pano.cfg.minrot) pano.rot/=2.f;
		else pano.rot*=-1.f;
	break;
	case PE_FLIPRIGHT:
		if(!pano.run) return 0;
		if(pano.rot<0.f) pano.rot*=-1.f;
	break;
	case PE_FLIPLEFT:
		if(!pano.run) return 0;
		if(pano.rot>0.f) pano.rot*=-1.f;
	break;
	case PE_MODE:     
		pano.mode = (pano.mode+1)%PM_NUM;
		if(!pano.pfish && pano.mode==PM_FISHEYE)
			pano.mode = (pano.mode+1)%PM_NUM;
	break;
	case PE_FISHMODE: pano.cfg.fm = (pano.cfg.fm+1)%3;  break;
	}
	return 1;
}

/* thread: dpl */
void panorun(){
	static Uint32 last=0;
	Uint32 now;
	struct img *img;
	if(!(img=panoactive())) return;
	if(!pano.run){ last=0; return; }
	now=SDL_GetTicks();
	if(last){
		float sec=(float)(now-last)/1000.f;
		dplevputp(DE_MOVE,pano.rot*sec,0.f);
	}
	last=now;
}
