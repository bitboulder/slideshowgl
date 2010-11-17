#include <stdlib.h>
#include <math.h>
#include <GL/gl.h>
#include <SDL.h>
#include <SDL_image.h>
#ifndef __WIN32__
	#include <ftgl.h>
#endif
#include "gl.h"
#include "main.h"
#include "sdl.h"
#include "dpl.h"
#include "img.h"
#include "load.h"
#include "cfg.h"
#include "exif.h"

enum dls { DLS_IMG, DLS_TXT };

struct gl {
	GLuint dls;
#ifndef __WIN32__
	FTGLfont *font;
#endif
} gl;

void glinit(){
	ldmaxtexsize();
	gl.dls=glGenLists(2);
	glNewList(gl.dls+DLS_IMG,GL_COMPILE);
	glBegin(GL_QUADS);
	glTexCoord2f( 0.0, 0.0); glVertex2f(-0.5,-0.5);
	glTexCoord2f( 1.0, 0.0); glVertex2f( 0.5,-0.5);
	glTexCoord2f( 1.0, 1.0); glVertex2f( 0.5, 0.5);
	glTexCoord2f( 0.0, 1.0); glVertex2f(-0.5, 0.5);
	glEnd();
	glEndList();
	glNewList(gl.dls+DLS_TXT,GL_COMPILE);
	glColor4f(0.8,0.8,0.8,0.7);
	glTranslatef(0.05,0.05,0.0);
	glScalef(0.9,0.9,1.0);
	glBegin(GL_QUADS);
	glVertex2f( 0.0, 0.0);
	glVertex2f( 1.0, 0.0);
	glVertex2f( 1.0, 1.0);
	glVertex2f( 0.0, 1.0);
	glEnd();
	glColor4f(0.0,0.0,0.0,1.0);
	glTranslatef(0.05,0.05,0.0);
	glScalef(0.9,0.9,1.0);
	glEndList();
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DITHER);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
#ifndef __WIN32__
	gl.font = ftglCreatePixmapFont(cfggetstr("gl.font"));
#endif
	ldcheckvartex();
}

void glfree(){
#ifndef __WIN32__
	if(gl.font) ftglDestroyFont(gl.font);
#endif
}

enum glmode { GLM_3D, GLM_2D, GLM_TXT };
void glmode(enum glmode dst){
	static enum glmode cur=-1;
	if(cur==dst) return;
	cur=dst;
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	switch(dst){
	case GLM_3D:  glFrustum(-1.0, 1.0, -sdl.scr_h, sdl.scr_h, 5.0, 60.0); break;
	case GLM_2D:  glOrtho(-0.5,0.5,0.5,-0.5,-1.,1.); break;
	case GLM_TXT: glOrtho( 0.0,1.0,1.0, 0.0,-1.,1.); break;
	}
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	if(dst==GLM_3D)
		glTranslatef(0.0, 0.0, -40.0);
	if(dst==GLM_TXT){
		glDisable(GL_TEXTURE_2D);
	}else{
		glEnable(GL_TEXTURE_2D);
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

void glrendermark(struct ipos *ipos,float rot){
	glDisable(GL_TEXTURE_2D);
	glRotatef(-rot,0.,0.,1.);
	glColor4f(1.,1.,1.,ipos->m*0.7);
	glTranslatef(.4,-.4,0.);
	glScalef(.1,.1,1.);
	glCallList(gl.dls+DLS_IMG);
	glEnable(GL_TEXTURE_2D);
}

void glrenderimg(struct img *img,char back){
	struct img *isc;
	struct ipos *ipos;
	struct iopt *iopt;
	float irat=imgldrat(img->ld);
	float srat=(float)sdl.scr_h/(float)sdl.scr_w;
	iopt=imgposopt(img->pos);
	if(!iopt->active) return;
	if(iopt->back!=back) return;
	if(!(isc=glseltex(img,iopt->tex))) return;
	ipos=imgposcur(img->pos);
	glPushMatrix();
	glTranslatef(ipos->x,ipos->y,0.);
	glScalef(ipos->s,ipos->s,1.);
	iopt=imgposopt(isc->pos);
	glColor4f(1.,1.,1.,ipos->a);
	{
		// get rotation near to 90°/270°
		float rot90 = ipos->r;
		while(rot90>= 90.) rot90-=180.;
		while(rot90< -90.) rot90+=180.;
		if(rot90<0.) rot90*=-1.;
		rot90/=90.;
		// rotate in real ratio
		if(srat<irat) glScalef(srat,1.,1.);
		else          glScalef(1.,1./srat,1.);
		glRotatef(ipos->r,0.,0.,1.);
		if(srat<irat) glScalef(1./irat,1.,1.);
		else          glScalef(1.,irat,1.);
		// correct size
		irat=powf(irat,rot90);
		glScalef(irat,irat,1.);
	}
	glCallList(gl.dls+DLS_IMG);
	if(ipos->m) glrendermark(ipos,imgexifrotf(img->exif));
	glPopMatrix();
}

void glrenderimgs(){
	int i;
	glmode(GLM_2D);
	for(i=0;i<nimg;i++) glrenderimg(imgs+i,1);
	for(i=0;i<nimg;i++) glrenderimg(imgs+i,0);
}

void gltextout(char *text,int size,float x,float y){
#ifndef __WIN32__
	if(!gl.font) return;
	ftglSetFontFaceSize(gl.font, size, size);
	glPushMatrix();
	glTranslatef(x,y,0.0);
	ftglRenderFont(gl.font, text, FTGL_RENDER_ALL);
	glPopMatrix();
#endif
}

void glrendertext(char *title,char *text){
	int i;
	glmode(GLM_TXT);
	glPushMatrix();
	glCallList(gl.dls+DLS_TXT);
	gltextout("Hello World!",24,0.0,0.0);
	printf("Title: %s\n",title);
	for(i=0;text[0];i+=2){
		printf("%s\t",text);
		/* TODO */
		text+=strlen(text)+1;
		printf("%s\n",text);
		text+=strlen(text)+1;
	}
	glPopMatrix();
}

void glrenderinfo(){
	struct img *img;
	char *info;
	if(!dplshowinfo()) return;
	if(!(img=imgget(dplgetimgi()))) return;
	if(!(info=imgexifinfo(img->exif))) return;
	glrendertext("Image info",info);
}

void glrenderhelp(){
	char *help;
	if((help=dplhelp())) glrendertext("Keyboardlayout:",help);
}

void glrenderinputnum(){
	int i=dplinputnum();
	if(!i) return;
	/* TODO */
}

void glpaint(){
	GLenum glerr;

	glClearColor(0.,0.,0.,1.);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glrenderimgs();
	glrenderinfo();
	glrenderhelp();
	
	glframerate();
	SDL_GL_SwapBuffers();

	if((glerr=glGetError())) error(ERR_CONT,"in glpaint (gl-err: %d)",glerr);
}

