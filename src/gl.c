#include "config.h"
#include <stdlib.h>
#include <math.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <SDL.h>
#include <SDL_image.h>
#if HAVE_FTGL
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
#include "pano.h"

enum dls { DLS_IMG, DLS_NUM };

struct gl {
	GLuint dls;
#if HAVE_FTGL
	FTGLfont *font;
#endif
	float bar;
	struct glcfg {
		float inputnum_lineh;
		float stat_lineh;
		float txt_bgcolor[4];
		float txt_fgcolor[4];
	} cfg;
} gl = {
	.bar = 0.f,
};

void glsetbar(float bar){ gl.bar=bar; }

void glinit(){
	char *fontfn;
	gl.cfg.inputnum_lineh = cfggetfloat("gl.inputnum_lineh");
	gl.cfg.stat_lineh     = cfggetfloat("gl.stat_lineh");
	cfggetcol("gl.txt_bgcolor",gl.cfg.txt_bgcolor);
	cfggetcol("gl.txt_fgcolor",gl.cfg.txt_fgcolor);
	ldmaxtexsize();
	gl.dls=glGenLists(DLS_NUM);
	glNewList(gl.dls+DLS_IMG,GL_COMPILE);
	glBegin(GL_QUADS);
	glTexCoord2f( 0.0, 0.0); glVertex2f(-0.5,-0.5);
	glTexCoord2f( 1.0, 0.0); glVertex2f( 0.5,-0.5);
	glTexCoord2f( 1.0, 1.0); glVertex2f( 0.5, 0.5);
	glTexCoord2f( 0.0, 1.0); glVertex2f(-0.5, 0.5);
	glEnd();
	glEndList();
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DITHER);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
#if HAVE_FTGL
	fontfn=finddatafile(cfggetstr("gl.font"));
	gl.font=fontfn ? ftglCreateBufferFont(fontfn) : NULL;
	if(gl.font) ftglSetFontFaceSize(gl.font, 24, 24);
#endif
	ldcheckvartex();
}

void glfree(){
#if HAVE_FTGL
	if(gl.font) ftglDestroyFont(gl.font);
#endif
}

float glmode(enum glmode dst,float h3d){
	static enum glmode cur=-1;
	static float cur_h3d;
	static float cur_w;
	float w = dst!=GLM_2D ? (float)sdl.scr_w/(float)sdl.scr_h : 1.f;
	if(cur==dst && (dst!=GLM_3D || h3d==cur_h3d) && w==cur_w) return w;
	cur=dst;
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	switch(dst){
	case GLM_3D:  gluPerspective(h3d, w, 5., 15.); break;
	case GLM_2D:  glOrtho(-0.5,0.5,0.5,-0.5,-1.,1.); break;
	case GLM_TXT: glOrtho(-0.5,0.5,-0.5,0.5,-1.,1.); break;
	}
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	if(dst==GLM_3D) gluLookAt(0.,0.,0., 0.,0.,1., 0.,-1.,0.);
	if(dst==GLM_TXT){
		glDisable(GL_TEXTURE_2D);
		glScalef(1.f/w,1.f,1.f);
	}else{
		glEnable(GL_TEXTURE_2D);
	}
	return w;
}

void glreshape(){
	glViewport(0, 0, (GLint)sdl.scr_w, (GLint)sdl.scr_h);
}

GLuint glseltex(struct img *img,enum imgtex it,struct img **isc){
	GLuint dl;
	*isc=img;
	if((dl=imgldtex(img->ld,it))) return dl;
	*isc=defimg;
	if((dl=imgldtex(defimg->ld,it))) return dl;
	return 0;
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

void gldrawimg(struct itx *tx){
	for(;tx->tex;tx++){
		glPushMatrix();
		glBindTexture(GL_TEXTURE_2D,tx->tex);
		glTranslatef(tx->x,tx->y,0.f);
		glScalef(tx->w,tx->h,1.f);
		glCallList(gl.dls+DLS_IMG);
		glPopMatrix();
	}
}

void glrenderimg(struct img *img,char back){
	struct img *isc;
	struct ipos *ipos;
	struct iopt *iopt=imgposopt(img->pos);
	struct icol *icol;
	GLuint dl;
	float irat=imgldrat(img->ld);
	float srat=(float)sdl.scr_h/(float)sdl.scr_w;
	if(!iopt->active) return;
	if(iopt->back!=back) return;
	if(!(dl=glseltex(img,iopt->tex,&isc))) return;
	ipos=imgposcur(img->pos);
	icol=imgposcol(img->pos);
	glPushMatrix();
	glTranslatef(ipos->x,ipos->y,0.);
	glScalef(ipos->s,ipos->s,1.);
	iopt=imgposopt(isc->pos);
	glColor4f(1.,1.,1.,ipos->a);
	// rotate in real ratio
	if(srat<irat) glScalef(srat,1.,1.);
	else          glScalef(1.,1./srat,1.);
	if(ipos->r) glRotatef(ipos->r,0.,0.,1.);
	if(srat<irat) glScalef(1./irat,1.,1.);
	else          glScalef(1.,irat,1.);
	if(ipos->r){
		// get rotation near to 90°/270°
		float rot90 = ipos->r;
		float schg=1.f;
		while(rot90>= 90.) rot90-=180.;
		while(rot90< -90.) rot90+=180.;
		if(rot90<0.) rot90*=-1.;
		rot90/=90.;
		// correct size
		if(srat>irat) if(srat>1.f/irat) schg=1.f/irat;
		              else              schg=srat;
		else          if(srat>1.f/irat) schg=1.f/srat;
		              else              schg=irat;
		schg=powf(schg,rot90);
		glScalef(schg,schg,1.);
	}
#if HAVE_GLACTIVETEXTURE
	// collor correction
	if(icol->b || icol->c){
		/* http://stackoverflow.com/questions/1506299/applying-brightness-and-contrast-with-opengl-es
		 * http://www.opengl.org/sdk/docs/man/xhtml/glGetTexEnv.xml
		 * http://www.opengl.org/sdk/docs/man/xhtml/glTexEnv.xml
		 */
		float b,c;
		GLint op1,op2;
		b=icol->b;
		c=-logf((1.f-icol->c)/2.f)/logf(2.f);
		if(icol->c>=0.f){
			b=b-.5f+.5f/c;
			op1 = b<0.f ? GL_SUBTRACT : GL_ADD;
			op2 = GL_MODULATE;
			b=fabs(b);
		}else{
			b=(b-.5f)*c+.5f;
			op1 = GL_MODULATE;
			op2 = b<0.f ? GL_SUBTRACT : GL_ADD;
			b=fabs(b);
		}
		glColor4f(b,b,b,c);

		glActiveTexture(GL_TEXTURE0);
		glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_COMBINE);
		glTexEnvi(GL_TEXTURE_ENV,GL_COMBINE_RGB,op1);
		glTexEnvi(GL_TEXTURE_ENV,GL_SRC0_RGB,GL_TEXTURE);
		glTexEnvi(GL_TEXTURE_ENV,GL_SRC1_RGB,GL_PRIMARY_COLOR);
		if(c<=1.f) glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND1_RGB,GL_SRC_ALPHA);
		glTexEnvi(GL_TEXTURE_ENV,GL_COMBINE_ALPHA,GL_REPLACE);
		glTexEnvi(GL_TEXTURE_ENV,GL_SRC0_ALPHA,GL_TEXTURE);

		glActiveTexture(GL_TEXTURE1);
		glEnable(GL_TEXTURE_2D);
		glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_COMBINE);
		glTexEnvi(GL_TEXTURE_ENV,GL_COMBINE_RGB,op2);
		glTexEnvi(GL_TEXTURE_ENV,GL_SRC0_RGB,GL_PREVIOUS);
		glTexEnvi(GL_TEXTURE_ENV,GL_SRC1_RGB,GL_PRIMARY_COLOR);
		if(c>1.f) glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND1_RGB,GL_SRC_ALPHA);
		glTexEnvi(GL_TEXTURE_ENV,GL_COMBINE_ALPHA,GL_REPLACE);
		glTexEnvi(GL_TEXTURE_ENV,GL_SRC0_ALPHA,GL_PREVIOUS);
	}
#endif
	// draw img
	glCallList(dl);
#if HAVE_GLACTIVETEXTURE
	if(icol->b || icol->c){
		glDisable(GL_TEXTURE_2D);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
		glActiveTexture(GL_TEXTURE0);
		glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
	}
#endif
	if(ipos->m) glrendermark(ipos,imgexifrotf(img->exif));
	glPopMatrix();
}

void glrenderimgs(){
	struct img *img;
	if(!imgs) return;
	glmode(GLM_2D,0.f);
	if(delimg) glrenderimg(delimg,1);
	for(img=*imgs;img;img=img->nxt) glrenderimg(img,1);
	for(img=*imgs;img;img=img->nxt) glrenderimg(img,0);
}

#if HAVE_FTGL
void gltextout(char *text,float x,float y){
	glPushMatrix();
	glTranslatef(x,y,0.0);
	ftglRenderFont(gl.font, text, FTGL_RENDER_ALL);
	glPopMatrix();
}
#endif

void glrendertext(char *title,char *text){
#if HAVE_FTGL
	/*
	 * w: .05 | .05 x .05 .75-x .05 | .05
	 * h: .05 | .05 .8 .05 | .05
	 */
	int i,j,lines=2;
	char *t;
	float maxw[2]={0.,0.};
	float w,h,x[2],y;
	float lineh;
	float winw;
	float tw,tx;
	if(!gl.font) return;
	for(t=text,i=0;t[0];i+=2,lines++) for(j=0;j<2;j++,t+=strlen(t)+1){
		float len=ftglGetFontAdvance(gl.font, t);
		if(len>maxw[j]) maxw[j]=len;
	}
	lineh=ftglGetFontLineHeight(gl.font);
	w=(maxw[0]+maxw[1])/.75f;
	h=(float)lines*lineh/.8f;
	winw=glmode(GLM_TXT,0.f);
	if(w/h<winw) glScalef(1.f/h, 1.f/h, 1.f);
	else         glScalef(winw/w,winw/w,1.f);
	glPushMatrix();
	glColor4fv(gl.cfg.txt_bgcolor);
	glRectf(-.45f*w, -.45f*h, .45f*w, .45f*h);
	x[0]=-.40f*w;
	x[1]=-.35f*w+maxw[0];
	y=.4*h-lineh;
	tw=ftglGetFontAdvance(gl.font,title);
	tx=-.375f*w+maxw[0]-tw/2.f;
	if(tx+tw>.4f) tx=.4f-tw;
	if(tx<-.4f) tx=-.4f;
	glColor4fv(gl.cfg.txt_fgcolor);
	gltextout(title,tx,y);
	y-=lineh*2;
	for(t=text,i=0;t[0];i+=2,y-=lineh) for(j=0;j<2;j++,t+=strlen(t)+1)
		gltextout(t,x[j],y);
	glPopMatrix();
#endif
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
#if HAVE_FTGL
	int i=dplinputnum();
	char txt[16];
	float lineh;
	float rect[6];
	if(!i) return;
	snprintf(txt,16,"%i",i);
	glmode(GLM_TXT,0.f);
	glPushMatrix();
	lineh=ftglGetFontLineHeight(gl.font);
	glScalef(gl.cfg.inputnum_lineh/lineh,gl.cfg.inputnum_lineh/lineh,1.f);
	ftglGetFontBBox(gl.font,txt,-1,rect);
	glTranslatef(-rect[0]-(rect[3]-rect[0])/2.f,
				 -rect[1]-(rect[4]-rect[1])/2.f,0.f);
	lineh*=0.2f;
	glColor4fv(gl.cfg.txt_bgcolor);
	glRectf(rect[0]-lineh,rect[1]-lineh,rect[3]+lineh,rect[4]+lineh);
	glColor4fv(gl.cfg.txt_fgcolor);
	ftglRenderFont(gl.font, txt, FTGL_RENDER_ALL);
	glPopMatrix();
#endif
}

void glrenderstat(){
#if HAVE_FTGL
	struct istat *stat=dplstat();
	float winw;
	float lineh;
	float rect[6];
	float tw,th;
	float bw,bh;
	if(!stat->h) return;
	winw=glmode(GLM_TXT,0.f);
	glPushMatrix();
	glTranslatef(-winw/2.f,-0.5f,0.f);
	lineh=ftglGetFontLineHeight(gl.font);
	glScalef(gl.cfg.stat_lineh/lineh,gl.cfg.stat_lineh/lineh,1.f);
	ftglGetFontBBox(gl.font,stat->txt,-1,rect);
	tw=rect[3]-rect[0];
	th=rect[4]-rect[1];
	bw=tw+lineh*0.4f;
	bh=th+lineh*0.4f;
	glTranslatef(0.f,(stat->h-1.f)*bh,0.f);
	glColor4fv(gl.cfg.txt_bgcolor);
	glRectf(0.f,0.f,bw,bh);
	glColor4fv(gl.cfg.txt_fgcolor);
	glTranslatef(-rect[0]+lineh*0.2f,-rect[1]+lineh*0.2f,0.f);
	ftglRenderFont(gl.font, stat->txt, FTGL_RENDER_ALL);
	glPopMatrix();
#endif
}

void glrenderbar(){
	if(!gl.bar) return;
	glmode(GLM_2D,0.f);
	glDisable(GL_TEXTURE_2D);
	glPushMatrix();
	glTranslatef(.5f,-.5f,0.f);
	glScalef(-.1f,.1f,1.f);
	glColor4f(.8f,.8f,.8f,.3f);
	glRectf(0.f,0.f,1.f,1.f);
	glTranslatef(.05f,.05f,0.f);
	glScalef(.9f,.9f,1.f);
	glColor4f(.8f,.8f,.8f,.7f);
	glRectf(0.f,0.f,gl.bar,1.f);
	glPopMatrix();
	glEnable(GL_TEXTURE_2D);
}

void glpaint(){
	GLenum glerr;

	glClearColor(0.,0.,0.,1.);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if(!panorender()) glrenderimgs();
	glrenderbar();
	glrenderstat();
	glrenderinfo();
	glrenderinputnum();
	glrenderhelp();
	
	if((glerr=glGetError())) error(ERR_CONT,"in glpaint (gl-err: %d)",glerr);
}

