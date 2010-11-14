#include <GL/gl.h>
#include <SDL.h>
#include "gl.h"
#include "main.h"
#include "sdl.h"
#include "dpl.h"
#include "img.h"
#include "load.h"

enum dls { DLS_IMG };

struct gl {
	GLuint dls;
} gl;

void glinit(){
	gl.dls=glGenLists(1);
	glNewList(gl.dls+DLS_IMG,GL_COMPILE);
	glBegin(GL_QUADS);
	glTexCoord2f( 0.0, 0.0);
	glVertex2f(  -0.5,-0.5);
	glTexCoord2f( 1.0, 0.0);
	glVertex2f(   0.5,-0.5);
	glTexCoord2f( 1.0, 1.0);
	glVertex2f(   0.5, 0.5);
	glTexCoord2f( 0.0, 1.0);
	glVertex2f(  -0.5, 0.5);
	glEnd();
	glEndList();
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DITHER);
	glDisable(GL_DEPTH_TEST);
}

enum glmode { GLM_3D, GLM_2D, GLM_TXT };
void glmode(enum glmode dst){
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	switch(dst){
	case GLM_3D:  glFrustum(-1.0, 1.0, -sdl.scr_h, sdl.scr_h, 5.0, 60.0); break;
	case GLM_2D:
	case GLM_TXT: glOrtho(-0.5,0.5,0.5,-0.5,-1.,1.); break;
	}
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	if(dst==GLM_3D)
		glTranslatef(0.0, 0.0, -40.0);
	if(dst==GLM_TXT){
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_BLEND);
	}else{
		glEnable(GL_TEXTURE_2D);
		glEnable(GL_BLEND);
	}
}

void glreshape(){
	glViewport(0, 0, (GLint)sdl.scr_w, (GLint)sdl.scr_h);
}

void glframerate(){
	static Uint32 t_last = 0;
	static Uint32 n = 0;
	Uint32 t_now = SDL_GetTicks();
	if(!t_last) t_last=t_now;
	else n++;
	if(t_now-t_last>1000){
		float fr = n/(float)(t_now-t_last)*1000.;
		debug(DBG_STA,"gl framerate %.1f fps",fr);
		t_last=t_now;
		n=0;
	}
}

struct img *glseltex(struct img *img,enum imgtex it){
	GLuint tex;
	if((tex=imgldtex(img->ld,it))){
		glBindTexture(GL_TEXTURE_2D, tex);
		return img;
	}
	if((tex=imgldtex(defimg.ld,it))){
		glBindTexture(GL_TEXTURE_2D, tex);
		return &defimg;
	}
	return NULL;
}

void glrenderimg(struct img *img,int pos){
	struct img *isc;
	struct ipos *ipos;
	if(!imgposactive(img->pos,pos)) return;
	if(!(isc=glseltex(img,imgpostex(img->pos,pos)))) return;
	if(!(ipos=imgposcur(img->pos,pos))) return;
	glPushMatrix();
	glTranslatef(ipos->x,ipos->y,0.);
	glScalef(ipos->s,ipos->s,1.);
	glScalef(imgldfitw(isc->ld),imgldfith(isc->ld),1.);
	glColor4f(1.,1.,1.,ipos->a);
	glCallList(gl.dls+DLS_IMG);
	glPopMatrix();
}

void glrenderimgs(){
	int i,p;
	glmode(GLM_2D);
	for(i=0;i<nimg;i++) for(p=0;p<2;p++)
		glrenderimg(imgs+i,p);
}

void glpaint(){
	glClearColor(0.,0.,0.,1.);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glrenderimgs();
	
	glframerate();
	SDL_GL_SwapBuffers();
}

