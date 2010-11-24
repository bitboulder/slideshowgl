#ifndef _GL_H
#define _GL_H

#include "load.h"

void glsetbar(float bar);

void glinit();
void glfree();
void glreshape();
void glpaint();

void gldrawimg(struct itx *tx);

#endif
