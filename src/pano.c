#include <stdlib.h>
#include <SDL.h>
#include <SDL_image.h>
#include <GL/gl.h>
#include <math.h>
#include "pano.h"
#include "img.h"
#include "dpl.h"
#include "load.h"
#include "main.h"
#include "help.h"
#include "sdl.h"
#include "gl.h"
#include "cfg.h"

#ifndef M_PI
# define M_PI		3.14159265358979323846	/* pi */
#endif
#define SIN(x)		sin((x)/180.f*M_PI)
#define COS(x)		cos((x)/180.f*M_PI)
#define TAN(x)		tan((x)/180.f*M_PI)

struct pano {
	float rot;
	char run;
	char plain;
	struct panocfg {
		float defrot;
		float minrot;
		float texdegree;
		float radius;
	} cfg;
} pano;

/* thread: sdl */
void panoinit(){
	pano.cfg.defrot=cfggetfloat("pano.defrot");
	pano.cfg.minrot=cfggetfloat("pano.minrot");
	pano.cfg.texdegree=cfggetfloat("pano.texdegree");
	pano.cfg.radius=cfggetfloat("pano.radius");
}

/* thread: dpl */
struct img *panoactive(){
	struct img *img;
	if(dplgetzoom()<=0) return NULL;
	if(!(img=imgget(dplgetimgi()))) return NULL;
	if(!imgldpano(img->ld)) return NULL;
	return img;
}
void panoplain(){ pano.plain=!pano.plain; }

/* thread: load */
void panores(struct img *img,struct ipano *ip,int w,int h,int *xres,int *yres){
	while(*xres>(float)w/ip->gw*pano.cfg.texdegree) *xres>>=1;
	while(*yres>(float)h/ip->gh*pano.cfg.texdegree) *yres>>=1;
}

/* thread: dpl, gl */
void panoperspect(struct ipano *ip,float spos,float *perspectw,float *perspecth){
	float tmp;
	float srat=sdlrat();
	if(spos<2.f) spos=powf(ip->gw/ip->gh/srat,2.f-spos);
	else spos=1.25f/powf(spos,logf(1.25f)/logf(2.f));
	if(!perspecth) perspecth=&tmp;
	*perspecth=ip->gh*spos;
	if(perspectw) *perspectw=*perspecth*srat;
}

/* thread: dpl */
char panospos2ipos(struct img *img,float sx,float sy,float *ix,float *iy){
	struct ipano *ip;
	float fitw,fith;
	float perspectw,perspecth;
	if(!img || img!=panoactive()) return 0;
	if(!(ip=imgldpano(img->ld))) return 0;
	panoperspect(ip,powf(2.f,(float)dplgetzoom()),&perspectw,&perspecth);
	fitw = ip->gw/perspectw;
	fith = ip->gh/perspecth;
	/* TODO: decrease fitw for not vertical edges */
	if(ix) *ix = sx/fitw;
	if(iy) *iy = sy/fith;
	return 1;
}

/* thread: dpl */
char panoclipx(struct img *img,float *xb){
	struct ipano *ip;
	float xs,ys,ymax,yr,yp,a,c,xrotmax,xpmax,xdec;
	if(!img || img!=panoactive()) return 1;
	if(!(ip=imgldpano(img->ld))) return 1;
	panoperspect(ip,powf(2.f,(float)dplgetzoom()),&xs,&ys);
	ymax=(ip->gh-ys)/2.f;
	yr=abs(ip->gyoff)+ymax;
	yp=ys/2.f;
	a=ys/2.f/TAN(ys/2.f);
	c=yp*COS(yr)+a*SIN(yr);
	xrotmax=xs/2;
	xpmax=sqrtf((yp*yp+a*a-c*c)/(1/SIN(xrotmax)/SIN(xrotmax)-1));
	xdec=xrotmax-xpmax;
	if(xdec>=0.f) *xb+=xdec/ip->gw;
	return ip->gw<360.f;
}

/* thread: dpl */
char panostart(float *x){
	struct img *img;
	struct ipano *ip;
	float perspectw;
	if(!(img=imgget(dplgetimgi()))) return 0;
	if(!(ip=imgldpano(img->ld))) return 0;
	pano.plain=0;
	pano.run=0;
	pano.rot=pano.cfg.defrot;
	if(ip->rotinit<0.f) pano.rot*=-1.f;
	panoperspect(ip,1.f,&perspectw,NULL);
	*x  = ip->rotinit<0.f ? .5f : -.5f;
	*x *= ip->gw/perspectw;
	return 1;
}


/* thread: gl */
void panovertex(double tx,double ty){
	float xyradius,x,y,z;
	xyradius=COS(ty)*pano.cfg.radius;
	y=SIN(ty)*pano.cfg.radius;
	x=SIN(tx)*xyradius;
	z=COS(tx)*xyradius;
	glVertex3d(x,y,z);
}

/* thread: gl */
void panodrawimg(struct itx *tx,struct ipano *ip){
	for(;tx->tex[0];tx++){
		glBindTexture(GL_TEXTURE_2D,tx->tex[0]);
		glBegin(GL_QUADS);
		glTexCoord2i(0,0); panovertex((tx->x-tx->w/2.)*ip->gw,(tx->y-tx->h/2.)*ip->gh-ip->gyoff);
		glTexCoord2i(1,0); panovertex((tx->x+tx->w/2.)*ip->gw,(tx->y-tx->h/2.)*ip->gh-ip->gyoff);
		glTexCoord2i(1,1); panovertex((tx->x+tx->w/2.)*ip->gw,(tx->y+tx->h/2.)*ip->gh-ip->gyoff);
		glTexCoord2i(0,1); panovertex((tx->x-tx->w/2.)*ip->gw,(tx->y+tx->h/2.)*ip->gh-ip->gyoff);
		glEnd();
	}
}

/* thread: gl */
char panorender(){
	struct img *img;
	struct ipano *ip;
	GLuint dl;
	struct ipos *ipos;
	char plain=pano.plain;
	float perspectw,perspecth;
	if(!(img=panoactive())) return 0;
	if(!(ip=imgldpano(img->ld))) return 0;
	if(!plain && !(dl=imgldtex(img->ld,TEX_PANO))) plain=1;
	if( plain && !(dl=imgldtex(img->ld,TEX_FULL))) return 0;
	ipos=imgposcur(img->pos);
	panoperspect(ip,ipos->s,&perspectw,&perspecth);
	glmode(plain?GLM_2D:GLM_3D, perspecth);
	glPushMatrix();
	glColor4f(1.,1.,1.,ipos->a);
	if(plain){
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
		glCallList(dl);
	}
	glPopMatrix();
	return 1;
}

/* thread: dpl */
char panoplay(){
	if(!panoactive()) return 0;
	pano.run=!pano.run;
	return 1;
}

/* thread: dpl */
char panospeed(int dir){
	if(!panoactive()) return 0;
	if(!pano.run) return 0;
	if(dir>0)
		if(pano.rot>0.f) pano.rot*=2.f;
		else if(pano.rot<-pano.cfg.minrot) pano.rot/=2.f;
		else pano.rot*=-1.f;
	else
		if(pano.rot<0.f) pano.rot*=2.f;
		else if(pano.rot>pano.cfg.minrot) pano.rot/=2.f;
		else pano.rot*=-1.f;
	return 1;
}

void panoflip(int dir){
	if(!panoactive()) return;
	if(!pano.run) return;
	if(pano.rot<0.f && dir<0) pano.rot*=-1.f;
	if(pano.rot>0.f && dir>0) pano.rot*=-1.f;
}

/* thread: dpl */
void panorun(){
	static Uint32 last=0;
	Uint32 now;
	if(!panoactive()) return;
	if(!pano.run){ last=0; return; }
	now=SDL_GetTicks();
	if(last){
		float sec=(float)(now-last)/1000.f;
		float sx=pano.rot*sec;
		float ix;
		if(panospos2ipos(panoactive(),sx,0.f,&ix,NULL))
			dplevputx(DE_MOVE,0,ix,0.f);
	}
	last=now;
}
