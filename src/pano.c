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

#ifndef M_PI
# define M_PI		3.14159265358979323846	/* pi */
#endif

struct pano {
	char run;
} pano = {
	.run=1,
};

void panores(struct img *img,struct ipano *ip,int w,int h,int *xres,int *yres){
	while(*xres>(float)w/ip->gw*5.f) *xres>>=1;
	while(*yres>(float)h/ip->gh*5.f) *yres>>=1;
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

char panorender(){
	int zoom;
	struct img *img;
	struct ipano *ip;
	GLuint dl;
	struct ipos *ipos;
	float perspect;
	if((zoom=dplgetzoom())<=0) return 0;
	if(!(img=imgget(dplgetimgi()))) return 0;
	if(!(ip=imgldpano(img->ld))) return 0;
	if(!(dl=imgldpanotex(img->ld))) return 0;
	ipos=imgposcur(img->pos);
	perspect=ip->gh*pow(1.25,(float)(1-zoom));
	glEnable(GL_TEXTURE_2D);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(perspect, (float)sdl.scr_w/(float)sdl.scr_h, 0.1, 100.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(0.,0.,0., 0.,0.,1., 0.,-1.,0.);
	glRotatef(ipos->y*ip->gh+ip->gyoff,-1.,0.,0.);
	glRotatef(ipos->x*ip->gw, 0.,1.,0.);
	glColor4f(1.,1.,1.,1.);
	glCallList(dl);
	return 1;
}

void panorun(){
#if 0
	if(!pano.ip) return;
	if(!pano.run) return;
	pano.ipos->x+=.01f;
#endif
}
