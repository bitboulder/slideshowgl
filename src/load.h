#ifndef _LOAD_H
#define _LOAD_H

#include <GL/glew.h>

struct itx {
	GLuint tex;
	float x,y;
	float w,h;
};

#include "img.h"

void ldmaxtexsize();
void ldcheckvartex();
void ldreset();

struct imgld *imgldinit(struct img *img);
void imgldfree(struct imgld *il);
GLuint imgldtex(struct imgld *il,enum imgtex it);
float imgldrat(struct imgld *il);

enum ldft {FT_RESET, FT_UPDATE};
void imgldfiletime(struct imgld *il,enum ldft act);

char ldtexload();
char ldffree(struct imgld *il,enum imgtex thold);

int ldthread(void *arg);

#endif
