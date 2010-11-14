#ifndef _LOAD_H
#define _LOAD_H

#include <GL/gl.h>
#include "img.h"

void ldforcefit();

struct imgld *imgldinit(char *fn);
void imgldfree(struct imgld * ip);
GLuint imgldtex(struct imgld *il,enum imgtex it);
float imgldfitw(struct imgld *il);
float imgldfith(struct imgld *il);

void ldtexload();

void *ldthread(void *arg);

void ldgetfiles(int argc,char **argv);

#endif
