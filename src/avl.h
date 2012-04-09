#ifndef _AVL_H
#define _AVL_h

#include "img.h"

struct avls;
struct avl;
typedef int (*avlcmp)(const struct avl *ia,const struct avl *ib);

struct avls *avlinit(avlcmp cmp);
void avlfree(struct avls *avls);

void avlins(struct avls *avls,struct img *img);
void avlout(struct avls *avls,struct img **first,struct img **last);
char avlprint(struct avls *avls,const char *pre);

int avlrndcmp(const struct avl *a,const struct avl *b);
int avlfilecmp(const struct avl *a,const struct avl *b);
int avldatecmp(const struct avl *a,const struct avl *b);

#endif
