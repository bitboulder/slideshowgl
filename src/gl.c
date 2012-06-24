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
#include "mark.h"
#include "map.h"
#include "hist.h"

enum dls { DLS_IMG, DLS_BRD, DLS_STOP, DLS_RUN, DLS_ARC, DLS_NUM };

enum glft { FT_NOR, FT_BIG, NFT };

struct glfont {
#if HAVE_FTGL
	FTGLfont *f;
#endif
	float h;
	float b;
};

struct gl {
	GLuint dls;
	GLuint prg;
	GLuint prgfish;
	GLuint movtex;
	struct glfont font[NFT];
	struct glfont *fontcur;
	float bar;
	struct glcfg {
		float hrat_input;
		float hrat_stat;
		float hrat_txtimg;
		float hrat_dirname;
		float hrat_cat;
		float txt_border;
		float dir_border;
		float col_txtbg[4];
		float col_txtfg[4];
		float col_txtti[4];
		float col_txtmk[4];
		float col_colmk[4];
		float col_dirname[4];
		float col_playicon[4];
		float prged_w;
		float prg_rat;
	} cfg;
	struct glsel {
		char act;
		int x,y;
		GLint view[4];
	} sel;
} gl = {
	.bar = 0.f,
};

void glcolora(float *col,float a){
	float c[4];
	int i;
	for(i=0;i<4;i++) c[i]=col[i];
	c[3]*=a;
	glColor4fv(c);
}

/* thread: all */
void glsetbar(float bar){ gl.bar=bar; sdlforceredraw(); }
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
	if(ret!=GL_TRUE){ error(ERR_CONT,"compiling fragment shader %s failed",fs_fn); info=fs; goto info_log; }
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
	float f;
	int i;
	if(!done){
		if(glewInit()!=GLEW_OK) error(ERR_QUIT,"glew init failed");
		if(cfggetint("cfg.version")){
			const char *str=NULL;
			if(GLEW_ARB_vertex_shader && GLEW_ARB_fragment_shader)
				str=(const char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
			mprintf("GL-Version: %s\n",glGetString(GL_VERSION));
			mprintf("GLSL-Version: %s\n",str?str:"NONE");
		}
		gl.cfg.hrat_input    = cfggetfloat("gl.hrat_input");
		gl.cfg.hrat_stat     = cfggetfloat("gl.hrat_stat");
		gl.cfg.hrat_txtimg   = cfggetfloat("gl.hrat_txtimg");
		gl.cfg.hrat_dirname  = cfggetfloat("gl.hrat_dirname");
		gl.cfg.hrat_cat      = cfggetfloat("gl.hrat_cat");
		gl.cfg.txt_border    = cfggetfloat("gl.txt_border");
		gl.cfg.dir_border    = cfggetfloat("gl.dir_border");
		cfggetcol("gl.col_txtbg",   gl.cfg.col_txtbg);
		cfggetcol("gl.col_txtfg",   gl.cfg.col_txtfg);
		cfggetcol("gl.col_txtti",   gl.cfg.col_txtti);
		cfggetcol("gl.col_txtmk",   gl.cfg.col_txtmk);
		cfggetcol("gl.col_colmk",   gl.cfg.col_colmk);
		cfggetcol("gl.col_dirname", gl.cfg.col_dirname);
		cfggetcol("gl.col_playicon",gl.cfg.col_playicon);
		gl.cfg.prged_w       = cfggetfloat("prged.w");
		gl.cfg.prg_rat       = cfggetfloat("prg.rat");
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

	glNewList(gl.dls+DLS_ARC,GL_COMPILE);
	glBegin(GL_TRIANGLE_FAN);
	glVertex2f(0.f,0.f);
	glVertex2f(0.f,1.f);
	for(f=0.05f;f<1.f;f+=0.05f) glVertex2f(f,cosf(f*M_PIf)/2.f+0.5f);
	glVertex2f(1.f,0.f);
	glEnd();
	glEndList();
	
	glDisable(GL_DITHER);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	memset(gl.font,0,sizeof(struct glfont)*NFT);
	fontfn=finddatafile(cfggetstr("gl.font"));
	if(fontfn) for(i=0;i<NFT;i++){
		unsigned int s = i ? 72 : 24;
#if HAVE_FTGL
		if(!(gl.font[i].f=ftglCreateBufferFont(fontfn))) continue;
		ftglSetFontFaceSize(gl.font[i].f, s, s);
		ftglSetFontCharMap(gl.font[i].f,FT_ENCODING_UNICODE);
		gl.font[i].h=ftglGetFontLineHeight(gl.font[i].f);
		gl.font[i].b=ftglGetFontDescender(gl.font[i].f);
#endif
	}
	ldcheckvartex();
	gl.prg=glprgload("vs.c","fs.c");
	gl.prgfish=glprgload("vs_fish.c","fs.c");

	gl.movtex=ldfile2tex(finddatafile("mov.png"));
}

void glfree(){
#if HAVE_FTGL
	int i;
	for(i=0;i<NFT;i++) if(gl.font[i].f) ftglDestroyFont(gl.font[i].f);
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
	if(gl.sel.act){
		gluPickMatrix(gl.sel.x,gl.sel.y,1.,1.,gl.sel.view);
		glScalef(1.f,-1.f,1.f);
	}
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

void glpostranslate(enum glpos pos,float w,float h){
	if(pos&GP_RIGHT  ) glTranslatef(-w,0.f,0.f);
	if(pos&GP_HCENTER) glTranslatef(-w/2.f,0.f,0.f);
	if(pos&GP_TOP    ) glTranslatef(0.f,-h,0.f);
	if(pos&GP_VCENTER) glTranslatef(0.f,-h/2.f,0.f);
}

#define glrect(w,h,p)	glrectarc(w,h,p,0.f);
void glrectarc(float w,float h,enum glpos pos,float barc){
	glPushMatrix();
	glpostranslate(pos,w,h);
	glRectf(0.f,0.f,w,h);
	if(barc){
		glTranslatef(0.f,h,0.f);
		glScalef(-barc,-h,1.f);
		glCallList(gl.dls+DLS_ARC);
		glScalef(-1.f,1.f,1.f);
		glTranslatef(w/barc,0.f,0.f);
		glCallList(gl.dls+DLS_ARC);
	}
	glPopMatrix();
}

char glfontsel(enum glft i){
	if(i>=NFT) return 0;
	while(i && !gl.font[i].h) i--;
	if(!gl.font[i].h) return 0;
	gl.fontcur=gl.font+i;
	return 1;
}

float glfontscale(float hrat,float wrat){
#if HAVE_FTGL
	if(gl.fontcur){
		glScalef(fabsf(hrat)/wrat/gl.fontcur->h,hrat/gl.fontcur->h,1.f);
		return gl.fontcur->h;
	}
#endif
	return 1.f;
}

float glfontwidth(const char *txt){
#if HAVE_FTGL
	if(gl.fontcur) return ftglGetFontAdvance(gl.fontcur->f,txt);
#endif
	return 1.f;
}

void glfontrender(const char *txt,enum glpos pos){
#if HAVE_FTGL
	if(!gl.fontcur) return;
	glPushMatrix();
	glpostranslate(pos,glfontwidth(txt),gl.fontcur->h);
	glTranslatef(0.f,-gl.fontcur->b,0.f);
	glmodeslave(GLM_TXT);
	ftglRenderFont(gl.fontcur->f,txt,FTGL_RENDER_ALL);
	glPopMatrix();
#endif
}

void glrendermov(float rat,float a){
	float bw=.05f,bh=.05f;
	const float bmax=.15f;
	if(!gl.movtex) return;
	glmodeslave(GLM_2D);
	if(gl.prg) glColor4f(.5f,.5f,.5f,a);
	else glColor4f(1.f,1.f,1.f,a);
	if(rat>=1.f){
		if((bh=bw*rat)>bmax) bw=(bh=bmax)/rat;
	}else{
		if((bw=bh/rat)>bmax) bh=(bw=bmax)*rat;
	}
	glBindTexture(GL_TEXTURE_2D,gl.movtex);
	glBegin(GL_QUADS);
	glTexCoord2f( 0.f,0.f); glVertex2f(-0.5f,-0.5f);
	glTexCoord2f(20.f,0.f); glVertex2f( 0.5f,-0.5f);
	glTexCoord2f(20.f,1.f); glVertex2f( 0.5f,-0.5f+bh);
	glTexCoord2f( 0.f,1.f); glVertex2f(-0.5f,-0.5f+bh);
	glTexCoord2f( 0.f,0.f); glVertex2f(-0.5f, 0.5f);
	glTexCoord2f(20.f,0.f); glVertex2f( 0.5f, 0.5f);
	glTexCoord2f(20.f,1.f); glVertex2f( 0.5f, 0.5f-bh);
	glTexCoord2f( 0.f,1.f); glVertex2f(-0.5f, 0.5f-bh);
	glEnd();
}

void glrenderact(float rat,char prgcol){
	float bw=.05f,bh=.05f;
	const float bmax=.15f;
	float *col=prgcol ? gl.cfg.col_colmk : gl.cfg.col_txtmk;
	glmodeslave(GLM_1D);
	glColor4f(col[0],col[1],col[2],.5f);
	if(rat>=1.f){
		if((bh=bw*rat)>bmax) bw=(bh=bmax)/rat;
	}else{
		if((bw=bh/rat)>bmax) bh=(bw=bmax)*rat;
	}
	glBegin(GL_QUADS);
	glVertex2f(-0.5f,   -0.5f); glVertex2f(-0.5f+bw,-0.5f); glVertex2f(-0.5f+bw, 0.5f   ); glVertex2f(-0.5f,    0.5f   );
	glVertex2f( 0.5f,   -0.5f); glVertex2f( 0.5f-bw,-0.5f); glVertex2f( 0.5f-bw, 0.5f   ); glVertex2f( 0.5f,    0.5f   );
	glVertex2f(-0.5f+bw,-0.5f); glVertex2f( 0.5f-bw,-0.5f); glVertex2f( 0.5f-bw,-0.5f+bh); glVertex2f(-0.5f+bw,-0.5f+bh);
	glVertex2f(-0.5f+bw, 0.5f); glVertex2f( 0.5f-bw, 0.5f); glVertex2f( 0.5f-bw, 0.5f-bh); glVertex2f(-0.5f+bw, 0.5f-bh);
	glEnd();
}

void glrendertxtimg(struct txtimg *txt,float a,char act){
	float col[4];
	float *prgcol=NULL;
	int i;
	char prgcolact;
	if(!glfontsel(FT_BIG)) return;
	for(i=0;i<4;i++) col[i]=txt->col[i];
	col[3]*=a;
	prgcolact=!act && effprgcolf(&prgcol)!=0.f && prgcol==txt->col;
	glPushMatrix();
	glColor4fv(col);
	glfontscale(-gl.cfg.hrat_txtimg,1.f);
	glfontrender(txt->txt,GP_CENTER);
	if(act || prgcolact){
		float w=glfontwidth(txt->txt);
		glScalef(w,gl.fontcur->h,1.f);
		glrenderact(w/gl.fontcur->h,prgcolact);
	}
	glPopMatrix();
}

void glrenderimgtext(const char *text,float irat,float a){
	float col[4];
	float w,h,wmax,wclip,hclip;
	size_t i,l,n=1;
	char buf[FILELEN],*pos;
	if(!text || !glfontsel(dplgetzoom()<-1 ? FT_NOR : FT_BIG)) return;

	for(i=l=0;l<FILELEN-1 && text[i];i++,l++) if(text[i]==' '){
		if(l && buf[l-1]=='\0') l--;
		else{ buf[l]='\0'; n++; }
	}else buf[l]=text[i];
	buf[l]='\0';

	for(i=0;i<4;i++) col[i]=gl.cfg.col_dirname[i];
	col[3]*=a;
	glColor4fv(col);

	glPushMatrix();
	glmodeslave(GLM_1D);
	glTranslatef(-0.0293f,0.0293f,1.f); /* render to center of top image (outof image center) */
	h=glfontscale(-gl.cfg.hrat_dirname,irat);
	hclip=h/gl.cfg.hrat_dirname*(1.f-2.f*gl.cfg.dir_border);
	wclip=hclip*irat;

	wmax=0;
	for(pos=buf,i=0;i<n;i++,pos+=strlen(pos)+1)
		if((w=glfontwidth(pos))>wmax) wmax=w;
	if(wmax>wclip) glScalef(wclip/wmax,1.f,1.f);
	if((float)n*h>hclip) glScalef(1.f,hclip/(float)n/h,1.f);

	glTranslatef(0.f,(float)(n-1)/2.f*h,0.f);
	for(pos=buf,i=0;i<n;i++,pos+=strlen(pos)+1){
		glfontrender(pos,GP_CENTER);
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

void glrendermark(float m,float rot,float irat){
	glPushMatrix();
	glRotatef(-rot,0.f,0.f,1.f);
	glTranslatef(.4f,-.4f,0.f);
	if(rot==90.f || rot==270.f) irat=1.f/irat;
	glScalef(1.f/irat,1.f,1.f);
	glmodeslave(GLM_1D);
	glColor4f(1.f,1.f,1.f,m);
	glScalef(.1f,.1f,1.f);
	glCallList(gl.dls+DLS_BRD);
	glmodeslave(GLM_1DI);
	glColor4f(m,m,m,m);
	glScalef(.9f,.9f,1.f);
	glCallList(gl.dls+DLS_IMG);
	glPopMatrix();
}

#define CB_TIME	0.2f
#define CB_SIZE	0.3f
float glcalcback(float ef){
	if(ef<CB_TIME)         return 1.f-ef/CB_TIME*(1.f-CB_SIZE);
	else if(ef<1.-CB_TIME) return CB_SIZE;
	else                   return 1.f-(1.f-ef)/CB_TIME*(1.f-CB_SIZE);
}

void glrenderimg(struct img *img,char layer,struct imglist *il,char act){
	struct ecur *ecur=imgposcur(img->pos);
	struct iopt *iopt=imgposopt(img->pos);
	struct icol *icol;
	GLuint dl=0;
	float irat=imgldrat(img->ld);
	float srat=sdlrat();
	struct txtimg *txt;
	float s;
	const char *mov;
	if(!ecur->act) return;
	if(iopt->layer!=layer) return;
	if(!irat) return;
	if(!(txt=imgfiletxt(img->file)) && !(dl=imgldtex(img->ld,iopt->tex))) return;
	if(!ecur->a || !ecur->s) return;
	icol=imgposcol(img->pos);
	glmodeslave(!ilprg(il) && ecur->a<1.f ? GLM_2DA : GLM_2D);
	glPushMatrix();
	if(il==ilget(CIL(1))){
		srat*=1.f-gl.cfg.prged_w;
		glTranslatef(gl.cfg.prged_w/2.f,0.f,0.f);
		glScalef(1.f-gl.cfg.prged_w,1.f,1.f);
	}
	if(ilprg(il)){
		float drat=gl.cfg.prg_rat;
		if(srat<drat) glScalef(1.f,srat/drat,1.f);
		if(srat>drat) glScalef(drat/srat,1.f,1.f);
		srat=drat;
	}
	glTranslatef(ecur->x,ecur->y,0.);
	s=ecur->s*glcalcback(ecur->back);
	glScalef(s,s,1.);
	if(gl.prg) glColor4f((icol->g+1.f)/2.f,(icol->c+1.f)/2.f,(icol->b+1.f)/2.f,ecur->a);
	else glColor4f(1.f,1.f,1.f,ecur->a);
	// rotate in real ratio
	if(srat>irat) glScalef(1.f/srat,1.f, 1.f);
	else          glScalef(1.f,     srat,1.f);
	if(ecur->r) glRotatef(ecur->r,0.,0.,1.);
	if(srat>irat) glScalef(irat,1.f,     1.f);
	else          glScalef(1.f, 1.f/irat,1.f);
	if(ecur->r){
		// get rotation near to 90°/270°
		float rot90 = ecur->r;
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
	if((mov=imgfilemov(img->file)) && mov[0]=='m') glrendermov(irat,ecur->a);
	if(txt) glrendertxtimg(txt,ecur->a,act);
	else if(act) glrenderact(irat,0);
	glrenderimgtext(imgfileimgtxt(img->file),irat,ecur->a);
	if(ecur->m) glrendermark(ecur->m,imgexifrotf(img->exif),irat);
	glPopMatrix();
}

int glrenderimgs1(struct img *img,int imgi,struct imglist *il,void *arg){
	glLoadName((GLuint)imgi+1);
	glrenderimg(img,*(char*)arg,il,dplgetactil(NULL)>=0 && imgi==ilcimgi(il));
	return 0;
}
void glrenderimgs(){
	char layer;
	glmode(GLM_2D);
	if(delimg) glrenderimg(delimg,1,0,0);
	for(layer=2;layer>=0;layer--) ilsforallimgs(glrenderimgs1,(void *)&layer,1,0);
	glLoadName(0);
}

void gltextout(const char *text,float x,float y){
#if HAVE_FTGL
	if(!gl.fontcur) return;
	glPushMatrix();
	glTranslatef(x,y,0.0);
	ftglRenderFont(gl.fontcur->f,text,FTGL_RENDER_ALL);
	glPopMatrix();
#endif
}

void glrendertext(const char *title,const char *text,float f){
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
	if(!glfontsel(FT_NOR)) return;
	for(t=text,i=0;t[0];i+=2,lines++) for(j=0;j<2;j++,t+=strlen(t)+1) if(t[0]){
		float len=glfontwidth(_(t));
		if(len>maxw[j]) maxw[j]=len;
	}
	lineh=gl.fontcur->h;
	w=(maxw[0]+maxw[1])/.75f;
	h=(float)lines*lineh/.8f;
	winw=glmode(GLM_TXT);
	if(w/h<winw) glScalef(1.f/h, 1.f/h, 1.f);
	else         glScalef(winw/w,winw/w,1.f);
	glPushMatrix();
	glcolora(gl.cfg.col_txtbg,f);
	glRectf(-.45f*w, -.45f*h, .45f*w, .45f*h);
	x[0]=-.40f*w;
	x[1]=-.35f*w+maxw[0];
	y=.4f*h-lineh;
	tw=glfontwidth(title);
	tx=-.375f*w+maxw[0]-tw/2.f;
	if(tx+tw>.4f) tx=.4f-tw;
	if(tx<-.4f) tx=-.4f;
	glcolora(gl.cfg.col_txtti,f);
	gltextout(title,tx,y);
	y-=lineh*2;
	for(t=text,i=0;t[0];i+=2,y-=lineh) for(j=0;j<2;j++,t+=strlen(t)+1) if(t[0]){
		if(!j && !t[strlen(t)+1]) glcolora(gl.cfg.col_txtti,f);
		else glcolora(gl.cfg.col_txtfg,f);
		if(!gl.sel.act) gltextout(_(t),x[j],y);
		else if(!j){
			glPushMatrix();
			glTranslatef(x[j],y-lineh*0.2f,0.f);
			glrect(.8f*w,lineh*0.9f,GP_BOTTOM|GP_LEFT);
			glPopMatrix();
		}
	}
	glPopMatrix();
#endif
}

void glrenderinfo(){
	char *info;
	float f;
	if(!(f=effswf(ESW_INFO))) return;
	if(!(info=dplgetinfo())) return;
	glrendertext(_("Image info"),info+ISLEN,f);
}

void glrenderinfosel(){
	char *info;
	float f,ffull,w,h,b,n=6.f;
	int i;
	if(!(f=effswf(ESW_INFOSEL))) return;
	if((ffull=effswf(ESW_INFO))){
		if(ffull>=1.f) return;
		else if((f*=1.f-ffull)<=0.f) return;
	}
	if(!(info=dplgetinfo())) return;
	w=glmode(GLM_TXT);
	glPushMatrix();
	glTranslatef(w/2.f,-0.5f,0.f);
	h=glfontscale(gl.cfg.hrat_cat,1.f);
	b=h*gl.cfg.txt_border*2.f;
	w*=80.f;
	glTranslatef(0.f,-(h*n+b)*(1.f-f),0.f);
	glColor4fv(gl.cfg.col_txtbg);
	glrect(w+b*2.f,h*n+b,GP_RIGHT|GP_BOTTOM);
	glColor4fv(gl.cfg.col_txtfg);
	glTranslatef(-b-w,(h+b)/2.f+h*(n-1),0.f);
	for(i=0;i<10;i++,info+=16){
		if(info[0]) glfontrender(info,GP_LEFT|GP_VCENTER);
		if(i<2 || i%2) glTranslatef(0.f,-h,0.f);
		if(i>1) glTranslatef((i%2?-1.f:1.f)*w/2.5f,0.f,0.f);
	}
	glPopMatrix();
}

void glrenderhelp(){
	const char *help=dplhelp();
	float f;
	if((f=effswf(ESW_HELP))) glrendertext(_("Interface"),help,f);
}

void glrendercat(){
	char *mark=NULL;
	char *cats,*cat;
	float f,w,h,b;
	float nc,hrat=gl.cfg.hrat_cat;
	GLuint name=IMGI_CAT+1;
	if(!(f=effswf(ESW_CAT))) return;
	if(!(cats=markcats())) return;
	if(!glfontsel(FT_NOR)) return;
	mark=imgposmark(ilcimg(NULL),MPC_NO);
	w=glmode(GLM_TXT);
	glPushMatrix();
	glTranslatef(-w/2.f,0.5f,0.f);
	for(w=0.f,nc=0.f,cat=cats;cat[0];cat+=FILELEN,nc++) if((b=glfontwidth(cat))>w) w=b;
	if(hrat*nc>.95f) hrat=.95f/nc;
	h=glfontscale(hrat,1.f);
	b=h*gl.cfg.txt_border*2.f;
	glScalef(f,1.f,1.f);
	glColor4fv(gl.cfg.col_txtbg);
	glrect(w+b,b/2.f,GP_TOP|GP_LEFT);
	glTranslatef(0.f,-b/2.f,0.f);
	for(cat=cats;cat[0];cat+=FILELEN,name++){
		if(mark) mark++;
		glColor4fv(gl.cfg.col_txtbg);
		glLoadName(name);
		glrect(w+b,h,GP_TOP|GP_LEFT);
		glLoadName(0);
		glTranslatef(b/2.f,-h/2.f,0.f);
		if(!gl.sel.act){
			if(mark && mark[0]) glColor4fv(gl.cfg.col_txtmk);
			else glcolora(gl.cfg.col_txtfg,.5f);
			glfontrender(cat,GP_VCENTER|GP_LEFT);
		}
		glTranslatef(-b/2.f,-h/2.f,0.f);
	}
	glColor4fv(gl.cfg.col_txtbg);
	glScalef(-(w+b),-h/2.f,1.f);
	glRotatef(90.f,0.f,0.f,1.f);
	glCallList(gl.dls+DLS_ARC);
	glPopMatrix();
}

void glrenderinput(){
	struct dplinput *in=dplgetinput();
	float w[3],wg,h,b;
	if(!in || !glfontsel(FT_NOR)) return;
	glmode(GLM_TXT);
	glPushMatrix();
	h=glfontscale(gl.cfg.hrat_input,1.f);
	w[0]=in->pre[0]  ? glfontwidth(in->pre)  : 0.f;
	w[1]=in->in[0]   ? glfontwidth(in->in)   : 0.f;
	w[2]=in->post[0] ? glfontwidth(in->post) : 0.f;
	wg=w[0]+w[1]+w[2];
	b=h*gl.cfg.txt_border*2.f;
	glTranslatef(-(wg+b)/2.f,0.f,0.f);
	glColor4fv(gl.cfg.col_txtbg);
	glrect(wg+b,h+b,GP_VCENTER|GP_LEFT);
	glTranslatef(b/2.f,0.f,0.f);
	glColor4fv(gl.cfg.col_txtmk);
	if(in->pre) glfontrender(in->pre,GP_VCENTER|GP_LEFT);
	glTranslatef(w[0],0.f,0.f);
	glColor4fv(gl.cfg.col_txtfg);
	if(in->in) glfontrender(in->in,GP_VCENTER|GP_LEFT);
	glTranslatef(w[1],0.f,0.f);
	glColor4fv(gl.cfg.col_txtmk);
	if(in->post) glfontrender(in->post,GP_VCENTER|GP_LEFT);
	glPopMatrix();
}

void glrenderstat(){
	struct istat *stat=effstat();
	const char *dir=ildir(NULL);
	float winw;
	float h,w,b;
	if(!stat->h || !glfontsel(FT_NOR)) return;
	winw=glmode(GLM_TXT);

	glPushMatrix();
	glTranslatef(-winw/2.f,-0.5f,0.f);
	h=glfontscale(gl.cfg.hrat_stat,1.f);
	w=glfontwidth(stat->txt);
	b=h*gl.cfg.txt_border*2.f;
	glTranslatef(0.f,-(h+b)*(1.f-stat->h),0.f);

	glColor4fv(gl.cfg.col_txtbg);
	glrect(w+b+h+b,h+b,GP_LEFT|GP_BOTTOM);

	glPushMatrix();
	glColor4fv(gl.cfg.col_playicon);
	glScalef(h+b,h+b,1.f);
	glTranslatef(.5f,.5f,0.f);
	glCallList(gl.dls+(stat->run?DLS_RUN:DLS_STOP));
	glPopMatrix();

	glColor4fv(gl.cfg.col_txtfg);
	glTranslatef(h+b,(h+b)/2.f,0.f);
	glfontrender(stat->txt,GP_LEFT|GP_VCENTER);
	glPopMatrix();

	if(!dir) return;
	glPushMatrix();
	glTranslatef(0.f,0.5f,0.f);
	h=glfontscale(gl.cfg.hrat_stat,1.f);
	w=glfontwidth(dir);
	b=h*gl.cfg.txt_border*2.f;
	glScalef(1.f,stat->h,1.f);
	glTranslatef(0.f,-(b+h)/2.f,0.f);

	glColor4fv(gl.cfg.col_txtbg);
	glrectarc(w,h+b,GP_CENTER,h+b);

	glColor4fv(gl.cfg.col_txtfg);
	glfontrender(dir,GP_CENTER);
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

void glrenderback(){
	char prged;
	int actil=dplgetactil(&prged);
	float srat=sdlrat()*(1.f-gl.cfg.prged_w);
	float drat=gl.cfg.prg_rat;
	float w;
	float col[4];
	int i;
	if(actil<0) return;
	w=glmode(GLM_TXT);
	for(i=0;i<3;i++) col[i]=gl.cfg.col_txtmk[i];
	col[3]=.5f;
	glPushMatrix();
	glColor4fv(col);
	glScalef(w,1.f,1.f);
	glRectf(actil?-.5f:.5f,-.5f,-.5f+gl.cfg.prged_w,.5f);
	if(actil && prged){
		glTranslatef(gl.cfg.prged_w/2.f,0.f,0.f);
		glScalef(1.f-gl.cfg.prged_w,1.f,1.f);
		if(srat<drat){
			glRectf(-.5f,-.5f,.5f,-srat/drat/2.f);
			glRectf(-.5f, .5f,.5f, srat/drat/2.f);
		}
		if(srat>drat){
			glRectf(-.5f,-.5f,-drat/srat/2.f,.5f);
			glRectf( .5f,-.5f, drat/srat/2.f,.5f);
		}
	}
	glPopMatrix();
}

void glrenderprgcol(){
	float *col;
	float colf=effprgcolf(&col);
	float w;
	int b,c;
	float colhsl[4];
	float chsl[4];
	float crgb[4];
	GLuint name=IMGI_COL+1;
	if(!colf || !col) return;
	w=glmode(GLM_TXT);
	glPushMatrix();
	glTranslatef(w/2.f,-.5f,0.f);
	glScalef(.1f,.4f,1.f);
	glScalef(colf,1.f,1.f);
	glTranslatef(-.5f,.5f,0.f);
	glColor4fv(gl.cfg.col_txtbg);
	glLoadName(name++);
	glrect(1.f,1.f,GP_CENTER);
	glScalef(1.f/5.f,1.f/20.f,1.f);
	glTranslatef(-1.f,9.f,0.f);
	col_rgb2hsl(colhsl,col);
	memcpy(chsl,colhsl,sizeof(float)*4);
	for(b=0;b<3;b++){
		int p=0;
		glPushMatrix();
		glScalef(1.f,(b?8.5f:18.f)/(float)NPRGCOL,1.f);
		for(c=0;c<NPRGCOL;c++){
			chsl[b]=((float)c+0.5f)/(float)NPRGCOL;
			if(chsl[b]<colhsl[b]) p=c;
			col_hsl2rgb(crgb,chsl);
			glColor4fv(crgb);
			glLoadName(name++);
			glrect(1.f,1.f,GP_CENTER);
			glTranslatef(0.f,-1.f,0.f);
		}
		glLoadName(0);
		chsl[b]=colhsl[b];
		glTranslatef(0.f,(float)(NPRGCOL-p),0.f);
		glColor4fv(col);
		glrect(2.f,b?2.f:1.f,GP_CENTER);
		glPopMatrix();
		if(!b) glTranslatef(2.f,0.f,0.f);
		else   glTranslatef(0.f,-9.5f,0.f);
	}
	glPopMatrix();
}

void glrenderhist(){
	float histf=effswf(ESW_HIST);
	float *hist=dplgethist();
	float w;
	int h,i;
	if(!histf || !hist) return;
	w=glmode(GLM_TXT);
	glPushMatrix();
	glTranslatef(w/2.f,.5f,0.f);
	glScalef(.1f,.8f,1.f);
	glScalef(histf,1.f,1.f);
	glTranslatef(-.5f,-.5f,0.f);
	glColor4fv(gl.cfg.col_txtbg);
	glrect(1.f,1.f,GP_CENTER);
//	glScalef(  .8f/.1f/w  ,1.f/45.f,1.f);
	glScalef(1.f/1.5f,1.f/45.f,1.f);
	glTranslatef(.5f,21.5f,0.f);
	for(h=0;h<HT_NUM;h++){
		glColor4fv(gl.cfg.col_txtbg);
		glrect(1.f,10.f,GP_TOP|GP_RIGHT);
		glPushMatrix();
		glScalef(1.f,10.f/(float)HDIM,1.f);
		switch(h){
		case 0: glColor4f(1.f,1.f,1.f,1.f); break;
		case 1: glColor4f(.9f,0.f,0.f,1.f); break;
		case 2: glColor4f(0.f,.7f,0.f,1.f); break;
		case 3: glColor4f(0.f,0.f,1.f,1.f); break;
		}
		for(i=0;i<HDIM;i++){
			glrect(hist[(h+1)*HDIM-i-1],1.f,GP_TOP|GP_RIGHT);
			glTranslatef(0.f,-1.f,0.f);
		}
		glPopMatrix();
		glTranslatef(0.f,-11.f,0.f);
	}
	glPopMatrix();
}

void glpaint(){
	GLenum glerr;

	glClearColor(0.,0.,0.,1.);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glrenderback();
	if(!panorender(gl.sel.act) && !maprender(gl.sel.act)) glrenderimgs();
	glrendercat();
	glrenderprgcol();
	if(gl.sel.act) return;
	glrenderbar();
	glrenderstat();
	glrenderhist();
	glrenderinfosel();
	glrenderinfo();
	glrenderinput();
	glrenderhelp();
	
	if((glerr=glGetError())) error(ERR_CONT,"in glpaint (gl-err: %d)",glerr);
}

int glselect(int x,int y){
	GLuint selbuf[64]={0};
	GLint hits,i;
	gl.sel.x=x;
	gl.sel.y=y;
	gl.sel.act=1;
	glSelectBuffer(64,selbuf);
	glGetIntegerv(GL_VIEWPORT,gl.sel.view);
	glRenderMode(GL_SELECT);
	glInitNames();
	glPushName(0);
	glpaint();
	hits=glRenderMode(GL_RENDER);
	gl.sel.act=0;
	for(i=hits-1;i>=0;i--) if(selbuf[4*i+3]) return (int)selbuf[4*i+3]-1;
	return -1;
}
