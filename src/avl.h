#ifndef _AVL_H
#define _AVL_h

#include "img.h"

struct avls;
struct avl;
typedef int (*avlcmp)(const struct avl *ia,const struct avl *ib);

struct avls *avlinit(avlcmp cmp,struct img **first,struct img **last);
void avlfree(struct avls *avls);

void avlins(struct avls *avls,struct img *img);
void avldel(struct avls *avls,struct img *img);
char avlchk(struct avls *avls,struct img *img);

int avlrndcmp(const struct avl *a,const struct avl *b);
int avlfilecmp(const struct avl *a,const struct avl *b);
int avldatecmp(const struct avl *a,const struct avl *b);

#endif
