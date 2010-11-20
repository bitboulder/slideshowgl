#ifndef _LOAD_H
#define _LOAD_H

#include <GL/gl.h>
#include "img.h"
#include "pano.h"

void ldmaxtexsize();
void ldcheckvartex();

struct imgld *imgldinit(struct img *img);
void imgldfree(struct imgld *il);
void imgldsetimg(struct imgld *il,struct img *img);
GLuint imgldtex(struct imgld *il,enum imgtex it);
float imgldrat(struct imgld *il);
char *imgldfn(struct imgld *il);
void imgldrefresh(struct imgld *il);
struct ipano *imgldpano(struct imgld *il);

char ldtexload();
char ldffree(struct imgld *il,enum imgtex thold);

void *ldthread(void *arg);

void ldgetfiles(int argc,char **argv);

#endif
