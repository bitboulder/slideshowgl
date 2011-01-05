#ifndef _GL_INT_H
#define _GL_INT_H

#include "gl.h"

GLuint glprgload(const char *vs_fn,const char *fs_fn);

enum glmode { GLM_3D, GLM_2D, GLM_TXT };
#define glmode(A)	glmodex(A,0.f,0)
float glmodex(enum glmode dst,float h3d,int fm);

void glinit(char done);
void glfree();
void glpaint();

#endif
