#ifndef _GL_H
#define _GL_H

#include "load.h"
#include <GL/glew.h>

void glsetbar(float bar);
char glprg();
GLuint glprgload(char *vs_fn,char *fs_fn);

enum glmode { GLM_3D, GLM_2D, GLM_TXT };
#define glmode(A)	glmodex(A,0.f,0)
float glmodex(enum glmode dst,float h3d,int fm);

void glinit(char done);
void glfree();
void glpaint();

void gldrawimg(struct itx *tx);

#endif
