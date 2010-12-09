#ifndef _LOAD_H
#define _LOAD_H

#include <GL/gl.h>

struct itx {
	GLuint tex[2];
	float x,y;
	float w,h;
};

#include "img.h"
#include "pano.h"

void ldmaxtexsize();
void ldcheckvartex();

struct imgld *imgldinit(struct img *img);
void imgldfree(struct imgld *il);
GLuint imgldtex(struct imgld *il,enum imgtex it);
float imgldrat(struct imgld *il);

char ldtexload();
char ldffree(struct imgld *il,enum imgtex thold);

int ldthread(void *arg);

#endif
