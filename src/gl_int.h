#ifndef _GL_INT_H
#define _GL_INT_H

#include "gl.h"

char glprgfish();

enum glmode { GLM_3D, GLM_3DS, GLM_3DP, GLM_2D, GLM_2DA, GLM_1D, GLM_1DI, GLM_TXT };
#define glmode(A)	glmodex(A,0.f,0)
float glmodex(enum glmode dst,float h3d,int fm);

void glinit(char done);
void glfree();
void glpaint();

int glselect(int x,int y);

#endif
