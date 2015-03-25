#ifndef _GL_RND_H
#define _GL_RND_H

#include "gl_int.h"

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
};

enum glft { FT_NOR, FT_BIG, NFT };
enum glpos {
	GP_LEFT    = 0x01,
	GP_RIGHT   = 0x02,
	GP_HCENTER = 0x04,
	GP_TOP     = 0x10,
	GP_BOTTOM  = 0x20,
	GP_VCENTER = 0x40,
};
#define GP_CENTER	(GP_HCENTER|GP_VCENTER)

struct glcfg *glcfg();
GLint glarg();
void glseccol(float r,float g,float b);
char glfontsel(enum glft i);
float glfontscale(float hrat,float wrat);
float glfontwidth(const char *txt);
#define glrect(w,h,p)	glrectarc(w,h,p,0.f);
void glrectarc(float w,float h,enum glpos pos,float barc);
void glfontrender(const char *txt,enum glpos pos);
void glmodeslave(enum glmode dst);


#endif
