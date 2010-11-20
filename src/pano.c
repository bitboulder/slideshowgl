#include <stdlib.h>
#include <SDL.h>
#include <SDL_image.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <math.h>
#include "pano.h"
#include "img.h"
#include "dpl.h"
#include "load.h"
#include "main.h"
#include "help.h"
#include "sdl.h"

/* TODO: join with load.c */
struct itex {
	GLuint tex;
	char loading;
	char thumb;
	char refresh;
};
struct sdlimg {
	SDL_Surface *sf;
	int ref;
};
void ldtexload_put(struct itex *itex,struct sdlimg *sdlimg);
struct sdlimg* sdlimg_gen(SDL_Surface *sf);
void sdlimg_unref(struct sdlimg *sdlimg);
/* TODO: join with load.c */

struct pimg {
	float gx,gy;
	float gw,gh;
	struct itex tex;
};

struct pano {
	struct ipano *ip;
	struct ipos *ipos;
	char initfailed;
	int npimg;
	struct pimg *pimgs;
	char run;
} pano = {
	.ip=NULL,
	.initfailed=0,
	.pimgs=NULL,
	.run=1,
};

void panoinit(struct img *img,struct ipano *ip){
	SDL_Surface *sdlimg;
	struct sdlimg *sdlimg2;
	int xres,yres,nx,ny,x,y;
	char *fn=imgldfn(img->ld);
	pano.ipos=imgposcur(img->pos);
	if(!(sdlimg=IMG_Load(fn))){ error(ERR_CONT,"Loading img failed \"%s\": %s",fn,IMG_GetError()); goto end; }
	if(sdlimg->format->BytesPerPixel!=3){ error(ERR_CONT,"Wrong pixelformat \"%s\"",fn); goto end; }
	for(xres=512;xres>(float)sdlimg->w/ip->gw*5.f;) xres/=2;
	for(yres=512;yres>(float)sdlimg->h/ip->gh*5.f;) yres/=2;
	nx=sdlimg->w/xres; if(nx*xres<sdlimg->w) nx++;
	ny=sdlimg->h/yres; if(ny*yres<sdlimg->h) ny++;
	debug(DBG_STA,"Pano init start res: %ix%i num %ix%i",xres,yres,nx,ny);
	pano.npimg=nx*ny;
	pano.pimgs=realloc(pano.pimgs,sizeof(struct pimg)*pano.npimg);
	for(x=0;x<nx;x++) for(y=0;y<ny;y++){
		SDL_Rect rsrc;
		struct pimg *p=pano.pimgs+x*ny+y;
		int w = x<nx-1 ? xres : sdlimg->w-xres*x;
		int h = y<ny-1 ? yres : sdlimg->h-yres*y;
		p->gx=(((float)x*xres+(float)w/2.f)/(float)sdlimg->w-.5f)*ip->gw;
		p->gy=(((float)y*yres+(float)h/2.f)/(float)sdlimg->h-.5f)*ip->gh;
		p->gw=(float)w/(float)sdlimg->w*ip->gw;
		p->gh=(float)h/(float)sdlimg->h*ip->gh;
		rsrc.x=x*xres; rsrc.w=w;
		rsrc.y=y*yres; rsrc.h=h;
		sdlimg2=sdlimg_gen(SDL_CreateRGBSurface(SDL_SWSURFACE,w,h, sdlimg->format->BitsPerPixel,
				sdlimg->format->Rmask, sdlimg->format->Gmask, sdlimg->format->Bmask, sdlimg->format->Amask));
		SDL_BlitSurface(sdlimg,&rsrc,sdlimg2->sf,NULL);
		p->tex.tex=0;
		ldtexload_put(&p->tex,sdlimg2);
	}
	SDL_FreeSurface(sdlimg);
	pano.ip=ip;
	debug(DBG_STA,"Pano init ready");
	return;
end:
	pano.initfailed=1;
}

void panofree(){
	int i;
	pano.ip=NULL;
	for(i=0;i<pano.npimg;i++) ldtexload_put(&pano.pimgs[i].tex,NULL);
	free(pano.pimgs);
	debug(DBG_STA,"Pano free ready");
	pano.pimgs=NULL;
}

void panocheck(){
	struct img *img=NULL;
	struct ipano *ip=NULL;
	if(dplgetzoom()>0 && (img=imgget(dplgetimgi()))) ip=imgldpano(img->ld);
	if(ip==pano.ip) return;
	if(pano.ip) panofree();
	if(!ip) pano.initfailed=0;
	else if(!pano.initfailed) panoinit(img,ip);
}

void panovertex(double tx,double ty){
	const float radius=10.f;
	double xyradius,x,y,z;
	tx*=M_PI/180.;
	ty*=M_PI/180.;
	xyradius=cos(ty)*radius;
	y=sin(ty)*radius;
	x=sin(tx)*xyradius;
	z=cos(tx)*xyradius;
	glVertex3d(x,y,z);
}

void panorenderpimg(struct pimg *p){
	glBindTexture(GL_TEXTURE_2D,p->tex.tex);
	glBegin(GL_QUADS);
	glTexCoord2i(0,0); panovertex(p->gx-p->gw/2.,p->gy-p->gh/2.);
	glTexCoord2i(1,0); panovertex(p->gx+p->gw/2.,p->gy-p->gh/2.);
	glTexCoord2i(1,1); panovertex(p->gx+p->gw/2.,p->gy+p->gh/2.);
	glTexCoord2i(0,1); panovertex(p->gx-p->gw/2.,p->gy+p->gh/2.);
	glEnd();
}

void panorender(){
	int i;
	float perspect;
	if(!pano.ip) return;
	perspect=pano.ip->gh*pow(1.25,(float)(1-dplgetzoom()));
	glEnable(GL_TEXTURE_2D);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(perspect, (float)sdl.scr_w/(float)sdl.scr_h, 0.1, 100.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(0.,0.,0., 0.,0.,1., 0.,-1.,0.);
	glRotatef(pano.ipos->y*pano.ip->gw,-1.,0.,0.);
	glRotatef(pano.ipos->x*pano.ip->gh, 0.,1.,0.);
	glColor4f(1.,1.,1.,1.);
	for(i=0;i<pano.npimg;i++) panorenderpimg(pano.pimgs+i);
}

void panorun(){
	if(!pano.ip) return;
	if(!pano.run) return;
	pano.ipos->x+=.01f;
}
