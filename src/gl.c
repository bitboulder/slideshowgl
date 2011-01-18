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

enum dls { DLS_IMG, DLS_BRD, DLS_STOP, DLS_RUN, DLS_NUM };

struct gl {
	GLuint dls;
	GLuint prg;
	GLuint prgfish;
#if HAVE_FTGL
	FTGLfont *font;
	FTGLfont *fontbig;
#endif
	float bar;
	struct glcfg {
		float hrat_inputnum;
		float hrat_stat;
		float hrat_txtimg;
		float hrat_dirname;
		float txt_border;
		float dir_border;
		float col_txtbg[4];
		float col_txtfg[4];
		float col_dirname[4];
		float col_playicon[4];
	} cfg;
} gl = {
	.bar = 0.f,
};

/* thread: all */
void glsetbar(float bar){ gl.bar=bar; }
char glprg(){ return !!gl.prg; }
char glprgfish(){ return !!gl.prgfish; }

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
		gl.cfg.hrat_inputnum = cfggetfloat("gl.hrat_inputnum");
		gl.cfg.hrat_stat     = cfggetfloat("gl.hrat_stat");
		gl.cfg.hrat_txtimg   = cfggetfloat("gl.hrat_txtimg");
		gl.cfg.hrat_dirname  = cfggetfloat("gl.hrat_dirname");
		gl.cfg.txt_border    = cfggetfloat("gl.txt_border");
		gl.cfg.dir_border    = cfggetfloat("gl.dir_border");
		cfggetcol("gl.col_txtbg",   gl.cfg.col_txtbg);
		cfggetcol("gl.col_txtfg",   gl.cfg.col_txtfg);
		cfggetcol("gl.col_dirname", gl.cfg.col_dirname);
		cfggetcol("gl.col_playicon",gl.cfg.col_playicon);
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

	glNewList(gl.dls+DLS_BRD,GL_COMPILE);
	glBegin(GL_QUADS);
	glVertex2f(-0.50f,-0.50f); glVertex2f(-0.45f,-0.50f); glVertex2f(-0.45f, 0.50f); glVertex2f(-0.50f, 0.50f);
	glVertex2f( 0.50f,-0.50f); glVertex2f( 0.45f,-0.50f); glVertex2f( 0.45f, 0.50f); glVertex2f( 0.50f, 0.50f);
	glVertex2f(-0.45f,-0.50f); glVertex2f( 0.45f,-0.50f); glVertex2f( 0.45f,-0.45f); glVertex2f(-0.45f,-0.45f);
	glVertex2f(-0.45f, 0.50f); glVertex2f( 0.45f, 0.50f); glVertex2f( 0.45f, 0.45f); glVertex2f(-0.45f, 0.45f);
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
	gl.prgfish=glprgload("vs_fish.c","fs.c");
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
	case GLM_3DP:
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_TEXTURE_2D);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);
		glSecondaryColor3f(1.f,0.f,0.f);
		glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
		if(dst==GLM_3DP && gl.prgfish) glUseProgram(gl.prgfish);
		else if(gl.prg) glUseProgram(gl.prg);
	break;
	case GLM_2D:
	case GLM_2DA:
		glDisable(GL_DEPTH_TEST);
		glEnable(GL_TEXTURE_2D);
		glDisable(GL_CULL_FACE);
		glSecondaryColor3f(1.f,0.f,0.f);
		if(dst==GLM_2DA) glBlendFunc(GL_SRC_ALPHA,GL_ONE);
		else glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
		if(gl.prg) glUseProgram(gl.prg);
	break;
	case GLM_1D:
	case GLM_1DI:
	case GLM_TXT:
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_CULL_FACE);
		glSecondaryColor3f(0.f,0.f,0.f);
		if(dst==GLM_1DI){
			glBlendFunc(GL_ONE_MINUS_DST_COLOR,GL_ONE_MINUS_SRC_ALPHA);
		}else{
			glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
		}
		if(gl.prg) glUseProgram(dst==GLM_TXT ? 0 : gl.prg);
	break;
	}
}

float glmodex(enum glmode dst,float h3d,int fm){
	static enum glmode cur=-1;
	static float cur_h3d;
	static float cur_w;
	static int cur_fm;
	float w = dst!=GLM_2D && dst!=GLM_2DA ? sdlrat() : 1.f;
	glmodeslave(dst);
	if(cur==dst && ((dst!=GLM_3D && dst!=GLM_3DP) || (h3d==cur_h3d && cur_fm==fm)) && w==cur_w) return w;
	cur=dst;
	cur_h3d=h3d;
	cur_fm=fm;
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	switch(dst){
	case GLM_3D:  gluPerspective(h3d, w, 1., 15.); break;
	case GLM_3DP: panoperspective(h3d,fm,w); break;
	case GLM_2D:
	case GLM_2DA:
		glOrtho(-0.5,0.5,0.5,-0.5,-1.,1.);
	break;
	case GLM_1D:
	case GLM_1DI:
	case GLM_TXT:
		glOrtho(-0.5,0.5,-0.5,0.5,-1.,1.);
	break;
	}
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	switch(dst){
	case GLM_3D:
	case GLM_3DP:
		gluLookAt(0.,0.,0., 0.,0.,1., 0.,-1.,0.);
	break;
	case GLM_2D:
	case GLM_2DA:
	break;
	case GLM_1D:
	case GLM_1DI:
	case GLM_TXT:
		glScalef(1.f/w,1.f,1.f);
	break;
	}
	return w;
}

enum glpos {
	GP_LEFT    = 0x01,
	GP_RIGHT   = 0x02,
	GP_HCENTER = 0x04,
	GP_TOP     = 0x10,
	GP_BOTTOM  = 0x20,
	GP_VCENTER = 0x40,
};
#define GP_CENTER	(GP_HCENTER|GP_VCENTER)

void glpostranslate(enum glpos pos,float *rect){
	if(pos&GP_LEFT   ) glTranslatef(-rect[0],0.f,0.f);
	if(pos&GP_RIGHT  ) glTranslatef(-rect[3],0.f,0.f);
	if(pos&GP_HCENTER) glTranslatef(-(rect[0]+rect[3])/2.f,0.f,0.f);
	if(pos&GP_TOP    ) glTranslatef(0.f,-rect[1],0.f);
	if(pos&GP_BOTTOM ) glTranslatef(0.f,-rect[4],0.f);
	if(pos&GP_VCENTER) glTranslatef(0.f,-(rect[1]+rect[4])/2.f,0.f);
}

void glrect(float w,float h,enum glpos pos){
	float rect[6];
	w/=2.f; h/=2.f;
	rect[0]=-w; rect[1]=-h; rect[2]=0.f;
	rect[3]= w; rect[4]= h; rect[5]=0.f;
	glPushMatrix();
	glpostranslate(pos,rect);
	glRectf(-w,-h,w,h);
	glPopMatrix();
}

float glfontscale(FTGLfont *font,float hrat,float wrat){
#if HAVE_FTGL
	float lineh=ftglGetFontLineHeight(font);
	glScalef(fabsf(hrat)/wrat/lineh,hrat/lineh,1.f);
	return lineh;
#else
	return 1.f;
#endif
}

float glfontwidth(FTGLfont *font,const char *txt){
#if HAVE_FTGL
	return ftglGetFontAdvance(font,txt);
#else
	return 1.f;
#endif
}

void glfontrender(FTGLfont *font,const char *txt,enum glpos pos){
#if HAVE_FTGL
	float rect[6];
	glPushMatrix();
	ftglGetFontBBox(font,txt,-1,rect);
	glpostranslate(pos,rect);
	glmodeslave(GLM_TXT);
	ftglRenderFont(font,txt,FTGL_RENDER_ALL);
	glPopMatrix();
#endif
}

void glrendertxtimg(struct txtimg *txt,float a){
	float col[4];
	int i;
	if(!gl.fontbig) return;
	for(i=0;i<4;i++) col[i]=txt->col[i];
	col[3]*=a;
	glPushMatrix();
	glColor4fv(col);
	glfontscale(gl.fontbig,-gl.cfg.hrat_txtimg,1.f);
	glfontrender(gl.fontbig,txt->txt,GP_CENTER);
	glPopMatrix();
}

void glrenderimgtext(const char *text,float irat,float a){
	float col[4];
	float w,h,wmax,wclip,hclip;
	size_t l,i,n=1;
	char buf[FILELEN],*pos;
	if(!text) return;
	if(!gl.fontbig) return;

	for(l=i=0;l<FILELEN-1 && text[i];i++,l++){
		if(text[i]=='.' && i>=2) break;
		switch(text[i]){
		case '/':
		case '\\': buf[l]=' '; break;
		case '_': buf[l]='\0'; n++; break;
		default: buf[l]=text[i]; break;
		}
	}
	while(l && buf[l-1]==' ') l--;
	buf[l]='\0';

	for(i=0;i<4;i++) col[i]=gl.cfg.col_dirname[i];
	col[3]*=a;
	glColor4fv(col);

	glPushMatrix();
	glTranslatef(-0.0293f,0.0293f,1.f); /* render to center of top image (outof image center) */
	h=glfontscale(gl.fontbig,-gl.cfg.hrat_dirname,irat);
	hclip=h/gl.cfg.hrat_dirname*(1.f-2.f*gl.cfg.dir_border);
	wclip=hclip*irat;

	wmax=0;
	for(pos=buf,i=0;i<n;i++,pos+=strlen(pos)+1)
		if((w=glfontwidth(gl.fontbig,pos))>wmax) wmax=w;
	if(wmax>wclip) glScalef(wclip/wmax,1.f,1.f);
	if((float)n*h>hclip) glScalef(1.f,hclip/(float)n/h,1.f);

	glTranslatef(0.f,(float)(n-1)/2.f*h,0.f);
	for(pos=buf,i=0;i<n;i++,pos+=strlen(pos)+1){
		glfontrender(gl.fontbig,pos,GP_CENTER);
		glTranslatef(0.f,-h,0.f);
	}
	glPopMatrix();
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

void glrendermark(struct ipos *ipos,float rot,float irat){
	glPushMatrix();
	glRotatef(-rot,0.f,0.f,1.f);
	glTranslatef(.4f,-.4f,0.f);
	if(rot==90.f || rot==270.f) irat=1.f/irat;
	glScalef(1.f/irat,1.f,1.f);
	glmodeslave(GLM_1D);
	glColor4f(1.f,1.f,1.f,ipos->m);
	glScalef(.1f,.1f,1.f);
	glCallList(gl.dls+DLS_BRD);
	glmodeslave(GLM_1DI);
	glColor4f(ipos->m,ipos->m,ipos->m,ipos->m);
	glScalef(.9f,.9f,1.f);
	glCallList(gl.dls+DLS_IMG);
	glPopMatrix();
}

void glrenderimg(struct img *img,char back){
	struct ipos *ipos;
	struct iopt *iopt=imgposopt(img->pos);
	struct icol *icol;
	GLuint dl=0;
	float irat=imgldrat(img->ld);
	float srat=sdlrat();
	struct txtimg *txt;
	if(!iopt->active) return;
	if(iopt->back!=back) return;
	if(!irat) return;
	if(!(txt=imgfiletxt(img->file)) && !(dl=imgldtex(img->ld,iopt->tex))) return;
	ipos=imgposcur(img->pos);
	icol=imgposcol(img->pos);
	glmodeslave(ipos->a<1.f ? GLM_2DA : GLM_2D);
	glPushMatrix();
	glTranslatef(ipos->x,ipos->y,0.);
	glScalef(ipos->s,ipos->s,1.);
	if(gl.prg) glColor4f((icol->g+1.f)/2.f,(icol->c+1.f)/2.f,(icol->b+1.f)/2.f,ipos->a);
	else glColor4f(1.f,1.f,1.f,ipos->a);
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
	if(dl) glCallList(dl);
	if(txt) glrendertxtimg(txt,ipos->a);
	glrenderimgtext(imgfiledir(img->file),irat,ipos->a);
	if(ipos->m) glrendermark(ipos,imgexifrotf(img->exif),irat);
	glPopMatrix();
}

void glrenderimgs(){
	struct img *img;
	char back;
	glmode(GLM_2D);
	if(delimg) glrenderimg(delimg,1);
	for(back=2;back>=0;back--)
		for(img=imgget(0);img;img=img->nxt) glrenderimg(img,back);
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
	glColor4fv(gl.cfg.col_txtbg);
	glRectf(-.45f*w, -.45f*h, .45f*w, .45f*h);
	x[0]=-.40f*w;
	x[1]=-.35f*w+maxw[0];
	y=.4f*h-lineh;
	tw=ftglGetFontAdvance(gl.font,title);
	tx=-.375f*w+maxw[0]-tw/2.f;
	if(tx+tw>.4f) tx=.4f-tw;
	if(tx<-.4f) tx=-.4f;
	glColor4fv(gl.cfg.col_txtfg);
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
	int i=dplinputnum();
	char txt[16];
	float w,h,b;
	if(!i) return;
	snprintf(txt,16,"%i",i);
	glmode(GLM_TXT);
	glPushMatrix();
	h=glfontscale(gl.font,gl.cfg.hrat_inputnum,1.f);
	w=glfontwidth(gl.font,txt);
	b=h*gl.cfg.txt_border*2.f;
	glColor4fv(gl.cfg.col_txtbg);
	glrect(w+b,h+b,GP_CENTER);
	glColor4fv(gl.cfg.col_txtfg);
	glfontrender(gl.font,txt,GP_CENTER);
	glPopMatrix();
}

void glrenderstat(){
	struct istat *stat=effstat();
	float winw;
	float h,w,b;
	if(!stat->h) return;
	winw=glmode(GLM_TXT);
	glPushMatrix();
	glTranslatef(-winw/2.f,-0.5f,0.f);
	h=glfontscale(gl.font,gl.cfg.hrat_stat,1.f);
	w=glfontwidth(gl.font,stat->txt);
	b=h*gl.cfg.txt_border*2.f;

	glColor4fv(gl.cfg.col_txtbg);
	glrect(w+b+h+b,h+b,GP_LEFT|GP_TOP);

	glPushMatrix();
	glColor4fv(gl.cfg.col_playicon);
	glScalef(h+b,h+b,1.f);
	glTranslatef(.5f,.5f,0.f);
	glCallList(gl.dls+(stat->run?DLS_RUN:DLS_STOP));
	glPopMatrix();

	glColor4fv(gl.cfg.col_txtfg);
	glTranslatef(h+b,(h+b)/2.f,0.f);
	glfontrender(gl.font,stat->txt,GP_LEFT|GP_VCENTER);

	glPopMatrix();
}

void glrenderbar(){
	float w;
	if(!gl.bar) return;
	w=glmode(GLM_1D);
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

