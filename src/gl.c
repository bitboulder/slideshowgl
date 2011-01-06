#include "config.h"
#include <stdlib.h>
#include <math.h>
#include <GL/glew.h>
#include <SDL.h>
#include <SDL_image.h>
#if HAVE_FTGL
	#include <ftgl.h>
#endif
#include "gl_int.h"
#include "main.h"
#include "sdl.h"
#include "dpl.h"
#include "img.h"
#include "load.h"
#include "cfg.h"
#include "exif.h"
#include "pano.h"
#include "eff.h"
#include "file.h"
#include "help.h"

enum dls { DLS_IMG, DLS_STOP, DLS_RUN, DLS_NUM };

struct gl {
	GLuint dls;
	GLuint prg;
#if HAVE_FTGL
	FTGLfont *font;
	FTGLfont *fontbig;
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

/* thread: all */
void glsetbar(float bar){ gl.bar=bar; }
char glprg(){ return !!gl.prg; }

char *textload(char *fn){
	FILE *fd;
	long len;
	char *buf = NULL;;
	if(!fn) return NULL;
	if(!(fd=fopen(fn,"r"))) return NULL;
	fseek(fd,0,SEEK_END);
	len=ftell(fd);
	if(len<=0) goto end;
	fseek(fd,0,SEEK_SET);
	buf=malloc((size_t)len+1);
	fread(buf,1,(size_t)len,fd);
	buf[len]='\0';
end:
	fclose(fd);
	return buf;
}


GLuint glprgload(const char *vs_fn,const char *fs_fn){
	char *vstxt,*fstxt;
	GLuint prg=0,vs=0,fs=0,info;
	GLint ret;
	if(!GLEW_ARB_vertex_shader || !GLEW_ARB_fragment_shader) return 0;
	if(!(vstxt=textload(finddatafile(vs_fn)))){ error(ERR_CONT,"loading vertex shader file %s failed",vs_fn); return 0; }
	if(!(fstxt=textload(finddatafile(fs_fn)))){ error(ERR_CONT,"loading fragment shader file %s failed",fs_fn); return 0; }
	vs=glCreateShader(GL_VERTEX_SHADER);
	fs=glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(vs,1,(const GLchar **)&vstxt,NULL);
	glShaderSource(fs,1,(const GLchar **)&fstxt,NULL);
	free((void*)vstxt);
	free((void*)fstxt);
	glCompileShader(vs);
	glCompileShader(fs);
	glGetShaderiv(vs,GL_COMPILE_STATUS,&ret);
	if(ret!=GL_TRUE){ error(ERR_CONT,"compiling vertex shader %s failed",vs_fn); info=vs; goto info_log; }
	glGetShaderiv(fs,GL_COMPILE_STATUS,&ret);
	if(ret!=GL_TRUE){ error(ERR_CONT,"compiling vertex shader %s failed",fs_fn); info=fs; goto info_log; }
	prg=glCreateProgram();
	glAttachShader(prg,vs);
	glAttachShader(prg,fs);
	glLinkProgram(prg);
	glGetProgramiv(prg,GL_LINK_STATUS,&ret);
	if(ret!=GL_TRUE){ error(ERR_CONT,"linking program (%s,%s) failed",vs_fn,fs_fn); info=prg; goto info_log; }
	return prg;
info_log:
	glGetObjectParameterivARB(info,GL_INFO_LOG_LENGTH,&ret);
	if(ret>0){
		char *buf=malloc((size_t)ret);
		glGetInfoLogARB(info,ret,&ret,buf);
		error(ERR_CONT,"InfoLog: %s",buf);
		free(buf);
	}
	if(prg){
		glDetachShader(prg,vs);
		glDetachShader(prg,fs);
		glDeleteProgram(prg);
	}
	if(vs) glDeleteShader(vs);
	if(fs) glDeleteShader(fs);
	return 0;
}

void glinit(char done){
	char *fontfn;
	if(!done){
		if(glewInit()!=GLEW_OK) error(ERR_QUIT,"glew init failed");
		if(cfggetint("cfg.version")){
			const char *str=NULL;
			if(GLEW_ARB_vertex_shader && GLEW_ARB_fragment_shader)
				str=(const char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
			mprintf("GL-Version: %s\n",glGetString(GL_VERSION));
			mprintf("GLSL-Version: %s\n",str?str:"NONE");
		}
		gl.cfg.inputnum_lineh = cfggetfloat("gl.inputnum_lineh");
		gl.cfg.stat_lineh     = cfggetfloat("gl.stat_lineh");
		cfggetcol("gl.txt_bgcolor",gl.cfg.txt_bgcolor);
		cfggetcol("gl.txt_fgcolor",gl.cfg.txt_fgcolor);
	}
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

	glNewList(gl.dls+DLS_STOP,GL_COMPILE);
	glBegin(GL_POLYGON);
	glVertex2f(-.25f,-.25f);
	glVertex2f(-.25f, .25f);
	glVertex2f( .25f, .25f);
	glVertex2f( .25f,-.25f);
	glEnd();
	glEndList();
	
	glNewList(gl.dls+DLS_RUN,GL_COMPILE);
	glBegin(GL_POLYGON);
	glVertex2f(-.25f,-.35f);
	glVertex2f(-.25f, .35f);
	glVertex2f( .25f,  0.f);
	glEnd();
	glEndList();
	
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DITHER);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
#if HAVE_FTGL
	fontfn=finddatafile(cfggetstr("gl.font"));
	if((gl.font=fontfn ? ftglCreateBufferFont(fontfn) : NULL)){
		ftglSetFontFaceSize(gl.font, 24, 24);
		ftglSetFontCharMap(gl.font,FT_ENCODING_UNICODE);
	}
	if((gl.fontbig=fontfn ? ftglCreateBufferFont(fontfn) : NULL)){
		ftglSetFontFaceSize(gl.fontbig, 72, 72);
		ftglSetFontCharMap(gl.fontbig,FT_ENCODING_UNICODE);
	}
#endif
	ldcheckvartex();
	gl.prg=glprgload("vs.c","fs.c");
}

void glfree(){
#if HAVE_FTGL
	if(gl.font)    ftglDestroyFont(gl.font);
	if(gl.fontbig) ftglDestroyFont(gl.fontbig);
#endif
}

void glmodeslave(enum glmode dst){
	static enum glmode cur=-1;
	if(cur==dst) return;
	cur=dst;
	switch(dst){
	case GLM_3D:
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_TEXTURE_2D);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);
		glSecondaryColor3f(1.f,1.f,1.f);
	break;
	case GLM_2D:
		glDisable(GL_DEPTH_TEST);
		glEnable(GL_TEXTURE_2D);
		glDisable(GL_CULL_FACE);
		glSecondaryColor3f(1.f,1.f,1.f);
	break;
	case GLM_TXT:
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_CULL_FACE);
		glSecondaryColor3f(0.f,0.f,0.f);
	break;
	}
}

float glmodex(enum glmode dst,float h3d,int fm){
	static enum glmode cur=-1;
	static float cur_h3d;
	static float cur_w;
	static int cur_fm;
	float w = dst!=GLM_2D ? sdlrat() : 1.f;
	glmodeslave(dst);
	if(cur==dst && (dst!=GLM_3D || (h3d==cur_h3d && cur_fm==fm)) && w==cur_w) return w;
	cur=dst;
	cur_h3d=h3d;
	cur_fm=fm;
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	switch(dst){
	case GLM_3D: 
		if(fm>=0) panoperspective(h3d,fm,w); else{
			gluPerspective(h3d, w, 1., 15.);
			if(gl.prg) glUseProgram(gl.prg);
		}
	break;
	case GLM_2D:
		glOrtho(-0.5,0.5,0.5,-0.5,-1.,1.);
		if(gl.prg) glUseProgram(gl.prg);
	break;
	case GLM_TXT:
		glOrtho(-0.5,0.5,-0.5,0.5,-1.,1.);
		if(gl.prg) glUseProgram(0);
	break;
	}
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	switch(dst){
	case GLM_3D:
		gluLookAt(0.,0.,0., 0.,0.,1., 0.,-1.,0.);
	break;
	case GLM_2D:
	break;
	case GLM_TXT:
		glScalef(1.f/w,1.f,1.f);
	break;
	}
	return w;
}

void glrendermark(struct ipos *ipos,float rot){
	glmodeslave(GLM_TXT);
	glPushMatrix();
	glRotatef(-rot,0.f,0.f,1.f);
	glColor4f(1.f,1.f,1.f,ipos->m*0.7f);
	glTranslatef(.4f,-.4f,0.f);
	glScalef(.1f,.1f,1.f);
	glCallList(gl.dls+DLS_IMG);
	glPopMatrix();
}

void glrenderimgtext(const char *text,float irat,float a){
	float rect[6];
	float s;
	if(!text) return;
	if(!gl.fontbig) return;
	// TODO: render text on an image-texture to prevent program switch and double-alpha
	//glmodeslave(GLM_TXT);
	if(gl.prg) glUseProgram(0);
	glPushMatrix();
	ftglGetFontBBox(gl.fontbig,text,-1,rect);
	glColor4f(1.f,1.f,1.f,a);
	// TODO: use irat
	s=MAX(rect[3]-rect[0],rect[4]-rect[1]);
	glScalef(.8f/s,-.8f/s,1.f);
	glTranslatef(-(rect[0]+rect[3])/2.f,-(rect[1]+rect[4])/2.f,0.f);
	ftglRenderFont(gl.fontbig,text,FTGL_RENDER_ALL);
	glPopMatrix();
	if(gl.prg) glUseProgram(gl.prg);
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
	struct ipos *ipos;
	struct iopt *iopt=imgposopt(img->pos);
	struct icol *icol;
	GLuint dl;
	float irat=imgldrat(img->ld);
	float srat=sdlrat();
	if(!iopt->active) return;
	if(iopt->back!=back) return;
	if(!(dl=imgldtex(img->ld,iopt->tex))) return;
	glmodeslave(GLM_2D);
	ipos=imgposcur(img->pos);
	icol=imgposcol(img->pos);
	glPushMatrix();
	glTranslatef(ipos->x,ipos->y,0.);
	glScalef(ipos->s,ipos->s,1.);
	if(gl.prg) glColor4f((icol->g+1.f)/2.f,(icol->c+1.f)/2.f,(icol->b+1.f)/2.f,ipos->a);
	else glColor4f(1.f,1.f,1.f,ipos->a);
	if(ipos->a<1.f) glBlendFunc(GL_SRC_ALPHA,GL_ONE);
	// rotate in real ratio
	if(srat>irat) glScalef(1.f/srat,1.f, 1.f);
	else          glScalef(1.f,     srat,1.f);
	if(ipos->r) glRotatef(ipos->r,0.,0.,1.);
	if(srat>irat) glScalef(irat,1.f,     1.f);
	else          glScalef(1.f, 1.f/irat,1.f);
	if(ipos->r){
		// get rotation near to 90°/270°
		float rot90 = ipos->r;
		float schg=1.f;
		while(rot90>= 90.f) rot90-=180.f;
		while(rot90< -90.f) rot90+=180.f;
		if(rot90<0.f) rot90*=-1.f;
		rot90/=90.f;
		// correct size
		if(srat<irat) if(srat<1.f/irat) schg=irat;
		              else              schg=1.f/srat;
		else          if(srat<1.f/irat) schg=srat;
		              else              schg=1.f/irat;
		schg=powf(schg,rot90);
		glScalef(schg,schg,1.);
	}
	// draw img
	glCallList(dl);
	glrenderimgtext(imgfiledir(img->file),irat,ipos->a);
	if(ipos->a<1.f) glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	if(ipos->m) glrendermark(ipos,imgexifrotf(img->exif));
	glPopMatrix();
}

void glrenderimgs(){
	struct img *img;
	glmode(GLM_2D);
	if(delimg) glrenderimg(delimg,1);
	for(img=imgget(0);img;img=img->nxt) glrenderimg(img,1);
	for(img=imgget(0);img;img=img->nxt) glrenderimg(img,0);
}

#if HAVE_FTGL
void gltextout(const char *text,float x,float y){
	glPushMatrix();
	glTranslatef(x,y,0.0);
	ftglRenderFont(gl.font,text,FTGL_RENDER_ALL);
	glPopMatrix();
}
#endif

void glrendertext(const char *title,const char *text){
#if HAVE_FTGL
	/*
	 * w: .05 | .05 x .05 .75-x .05 | .05
	 * h: .05 | .05 .8 .05 | .05
	 */
	int i,j,lines=2;
	const char *t;
	float maxw[2]={0.,0.};
	float w,h,x[2],y;
	float lineh;
	float winw;
	float tw,tx;
	if(!gl.font) return;
	for(t=text,i=0;t[0];i+=2,lines++) for(j=0;j<2;j++,t+=strlen(t)+1) if(t[0]){
		float len=ftglGetFontAdvance(gl.font,_(t));
		if(len>maxw[j]) maxw[j]=len;
	}
	lineh=ftglGetFontLineHeight(gl.font);
	w=(maxw[0]+maxw[1])/.75f;
	h=(float)lines*lineh/.8f;
	winw=glmode(GLM_TXT);
	if(w/h<winw) glScalef(1.f/h, 1.f/h, 1.f);
	else         glScalef(winw/w,winw/w,1.f);
	glPushMatrix();
	glColor4fv(gl.cfg.txt_bgcolor);
	glRectf(-.45f*w, -.45f*h, .45f*w, .45f*h);
	x[0]=-.40f*w;
	x[1]=-.35f*w+maxw[0];
	y=.4f*h-lineh;
	tw=ftglGetFontAdvance(gl.font,title);
	tx=-.375f*w+maxw[0]-tw/2.f;
	if(tx+tw>.4f) tx=.4f-tw;
	if(tx<-.4f) tx=-.4f;
	glColor4fv(gl.cfg.txt_fgcolor);
	gltextout(title,tx,y);
	y-=lineh*2;
	for(t=text,i=0;t[0];i+=2,y-=lineh) for(j=0;j<2;j++,t+=strlen(t)+1) if(t[0])
		gltextout(_(t),x[j],y);
	glPopMatrix();
#endif
}

void glrenderinfo(){
	struct img *img;
	char *info;
	if(!dplshowinfo()) return;
	if(!(img=imgget(dplgetimgi()))) return;
	if(!(info=imgexifinfo(img->exif))) return;
	glrendertext(_("Image info"),info);
}

void glrenderhelp(){
	const char *help;
	if((help=dplhelp())) glrendertext(_("Interface"),help);
}

void glrenderinputnum(){
#if HAVE_FTGL
	int i=dplinputnum();
	char txt[16];
	float lineh;
	float rect[6];
	if(!i) return;
	snprintf(txt,16,"%i",i);
	glmode(GLM_TXT);
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
	ftglRenderFont(gl.font,txt,FTGL_RENDER_ALL);
	glPopMatrix();
#endif
}

void glrenderstat(){
#if HAVE_FTGL
	struct istat *stat=effstat();
	float winw;
	float lineh;
	float rect[6];
	float tw,th;
	float bw,bh;
	if(!stat->h) return;
	winw=glmode(GLM_TXT);
	glPushMatrix();
	glTranslatef(-winw/2.f,-0.5f,0.f);
	lineh=ftglGetFontLineHeight(gl.font);
	glScalef(gl.cfg.stat_lineh/lineh,gl.cfg.stat_lineh/lineh,1.f);
	ftglGetFontBBox(gl.font,stat->txt,-1,rect);
	tw=rect[3]-rect[0];
	th=rect[4]-rect[1];
	bh=th+lineh*0.4f;
	bw=tw+lineh*0.4f+bh;
	glTranslatef(0.f,(stat->h-1.f)*bh,0.f);

	glColor4fv(gl.cfg.txt_bgcolor);
	glRectf(0.f,0.f,bw,bh);

	glPushMatrix();
	glColor4f(0.f,0.f,0.f,.7f);
	glScalef(bh,bh,1.f);
	glTranslatef(.5f,.5f,0.f);
	glCallList(gl.dls+(stat->run?DLS_RUN:DLS_STOP));
	glPopMatrix();

	glColor4fv(gl.cfg.txt_fgcolor);
	glTranslatef(-rect[0]+lineh*0.2f+bh,-rect[1]+lineh*0.2f,0.f);
	ftglRenderFont(gl.font,stat->txt,FTGL_RENDER_ALL);
	glPopMatrix();
#endif
}

void glrenderbar(){
	float w;
	if(!gl.bar) return;
	w=glmode(GLM_TXT);
	glPushMatrix();
	glScalef(w,-1.f,1.f);
	glTranslatef(.5f,-.5f,0.f);
	glScalef(-.1f,.1f,1.f);
	glColor4f(.8f,.8f,.8f,.3f);
	glRectf(0.f,0.f,1.f,1.f);
	glTranslatef(.05f,.05f,0.f);
	glScalef(.9f,.9f,1.f);
	glColor4f(.8f,.8f,.8f,.7f);
	glRectf(0.f,0.f,gl.bar,1.f);
	glPopMatrix();
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

