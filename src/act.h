#ifndef _ACT_H
#define _ACT_H

#include "img.h"

enum act { ACT_SAVEMARKS, ACT_ROTATE, ACT_DELETE, ACT_DELORI, ACT_ILCLEANUP, ACT_NUM };

void actadd(enum act act,struct img *img);
int actthread(void *arg);
void actforce();

#endif
