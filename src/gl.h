#ifndef _GL_H
#define _GL_H

#include "load.h"

void glinit();
void glfree();
void glreshape();
void glpaint();

void gldrawimg(struct itx *tx);

#endif
