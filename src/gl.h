#ifndef _GL_H
#define _GL_H

#include "load.h"

void glsetbar(float bar);

enum glmode { GLM_3D, GLM_2D, GLM_TXT };
float glmode(enum glmode dst,float h3d);

void glinit();
void glfree();
void glpaint();

void gldrawimg(struct itx *tx);

#endif
