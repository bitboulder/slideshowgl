#ifndef _LOAD_H
#define _LOAD_H

#include <GL/glew.h>

struct itx {
	GLuint tex;
	float x,y;
	float w,h;
};

#include "img.h"
#include "il.h"

void ldmaxtexsize();
void ldcheckvartex();
void ldreset();

struct imgld *imgldinit(struct img *img);
void imgldfree(struct imgld *il);
GLuint imgldtex(struct imgld *il,enum imgtex it);
float imgldrat(struct imgld *il);
char imgldwh(struct imgld *il,float *w,float *h);

struct ldft { unsigned int ftchk; long ft; };
char ldfiletime(struct ldft *lf,enum eldft act,char *fn);
char imgldfiletime(struct imgld *il,enum eldft act);

char ldtexload();
GLuint ldfile2tex(const char *fn);
char ldffree(struct imgld *il,enum imgtex thold);

int ldthread(void *arg);

#endif
