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

struct pano {
	struct img *active;
	float perspectw;
	float perspecth;
	float rot;
	char run;
	char plain;
	struct panocfg {
		float defrot;
		float minrot;
		float texdegree;
		float radius;
	} cfg;
} pano = {
	.active     = NULL,
};

/* thread: sdl */
void panoinit(){
	pano.cfg.defrot=cfggetfloat("pano.defrot");
	pano.cfg.minrot=cfggetfloat("pano.minrot");
	pano.cfg.texdegree=cfggetfloat("pano.texdegree");
	pano.cfg.radius=cfggetfloat("pano.radius");
}

/* thread: dpl */
char panoactive(){ return pano.active!=NULL; }
void panoplain(){ pano.plain=!pano.plain; }

/* thread: load */
void panores(struct img *img,struct ipano *ip,int w,int h,int *xres,int *yres){
	while(*xres>(float)w/ip->gw*pano.cfg.texdegree) *xres>>=1;
	while(*yres>(float)h/ip->gh*pano.cfg.texdegree) *yres>>=1;
}

/* thread: dpl */
char panospos2ipos(struct img *img,float sx,float sy,float *ix,float *iy){
	struct ipano *ip;
	float fitw,fith;
	if(!img || img!=pano.active) return 0;
	if(!(ip=imgldpano(img->ld))) return 0;
	fitw = ip->gw/pano.perspectw;
	fith = ip->gh/pano.perspecth;
	/* TODO: decrease fitw for not vertical edges */
	if(ix) *ix = sx/fitw;
	if(iy) *iy = sy/fith;
	return 1;
}

/* thread: dpl */
char panoclipx(struct img *img){
	struct ipano *ip;
	if(img!=pano.active) return 1;
	if(!(ip=imgldpano(img->ld))) return 1;
	return ip->gw<360.f;
}

/* thread: gl */
void panovertex(double tx,double ty){
	float xyradius,x,y,z;
	tx*=M_PI/180.f;
	ty*=M_PI/180.f;
	xyradius=cos(ty)*pano.cfg.radius;
	y=sin(ty)*pano.cfg.radius;
	x=sin(tx)*xyradius;
	z=cos(tx)*xyradius;
	glVertex3d(x,y,z);
}

/* thread: gl */
void panodrawimg(struct itx *tx,struct ipano *ip){
	for(;tx->tex;tx++){
		glBindTexture(GL_TEXTURE_2D,tx->tex);
		glBegin(GL_QUADS);
		glTexCoord2i(0,0); panovertex((tx->x-tx->w/2.)*ip->gw,(tx->y-tx->h/2.)*ip->gh-ip->gyoff);
		glTexCoord2i(1,0); panovertex((tx->x+tx->w/2.)*ip->gw,(tx->y-tx->h/2.)*ip->gh-ip->gyoff);
		glTexCoord2i(1,1); panovertex((tx->x+tx->w/2.)*ip->gw,(tx->y+tx->h/2.)*ip->gh-ip->gyoff);
		glTexCoord2i(0,1); panovertex((tx->x-tx->w/2.)*ip->gw,(tx->y+tx->h/2.)*ip->gh-ip->gyoff);
		glEnd();
	}
}

/* thread: gl */
void panoinitpos(struct ipano *ip){
	float xpos = ip->rotinit<0.f ? .5f : -.5f;
	xpos*=ip->gw/pano.perspectw;
	dplevputx(DE_MOVE,0,xpos,0.f);
	pano.rot=pano.cfg.defrot;
	if(ip->rotinit<0.f) pano.rot*=-1.f;
	pano.run=1;
}

/* thread: gl */
char panorender(){
	int zoom;
	struct img *img;
	struct ipano *ip;
	GLuint dl;
	struct ipos *ipos;
	char ret=0;
	struct itx *tx;
	if((zoom=dplgetzoom())<=0) goto end;
	if(!(img=imgget(dplgetimgi()))) goto end;
	if(!(ip=imgldpano(img->ld))) goto end;
	if(!(dl=imgldpanotex(img->ld,&tx))) goto end;
	if(!pano.active) pano.plain=0;
	ipos=imgposcur(img->pos);
	pano.perspecth=ip->gh*powf(1.25f,(float)(1-zoom));
	pano.perspectw=pano.perspecth*glmode(pano.plain?GLM_2D:GLM_3D, pano.perspecth);
	glPushMatrix();
	if(pano.plain){
		float x=-ipos->x;
		while(x<0.f) x+=1.f;
		while(x>1.f) x-=1.f;
		glScalef(ip->gw/pano.perspectw,ip->gh/pano.perspecth,1.f);
		glTranslatef(x,-ipos->y,0.f);
		gldrawimg(tx);
		glTranslatef(-1.f,0.f,0.f);
		gldrawimg(tx);
	}else{
		glRotatef(-ipos->y*ip->gh+ip->gyoff,-1.,0.,0.);
		glRotatef(ipos->x*ip->gw, 0.,-1.,0.);
		glColor4f(1.,1.,1.,1.);
		glCallList(dl);
	}
	glPopMatrix();
	ret=1;
end:
	if(!pano.active && ret){
		pano.active=img;
		panoinitpos(ip);
	}else if(pano.active && !ret) pano.active=NULL;
	return ret;
}

/* thread: dpl */
char panoplay(){
	if(!pano.active) return 0;
	pano.run=!pano.run;
	return 1;
}

/* thread: dpl */
char panospeed(int dir){
	if(!pano.active) return 0;
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
	if(!pano.active) return;
	if(!pano.run) return;
	if(pano.rot<0.f && dir<0) pano.rot*=-1.f;
	if(pano.rot>0.f && dir>0) pano.rot*=-1.f;
}

/* thread: dpl */
void panorun(){
	static Uint32 last=0;
	Uint32 now;
	if(!pano.active) return;
	if(!pano.run){ last=0; return; }
	now=SDL_GetTicks();
	if(last){
		float sec=(float)(now-last)/1000.f;
		float sx=pano.rot*sec;
		float ix;
		if(panospos2ipos(pano.active,sx,0.f,&ix,NULL))
			dplevputx(DE_MOVE,0,ix,0.f);
	}
	last=now;
}
