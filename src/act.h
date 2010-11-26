#ifndef _ACT_H
#define _ACT_H

#include "img.h"

enum act { ACT_LOADMARKS, ACT_SAVEMARKS, ACT_ROTATE, ACT_DELETE };

void actadd(enum act act,struct img *img);
int actthread(void *arg);

#endif
