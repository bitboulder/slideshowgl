#ifndef _AVL_H
#define _AVL_h

#include "img.h"

enum avlcmp {ILS_DATE, ILS_FILE, ILS_NONE, ILS_RND, ILS_NUM};

struct avls;
struct avl;

struct avls *avlinit(enum avlcmp cmp,struct img **first,struct img **last);
void avlfree(struct avls *avls);
char avlsortchg(struct avls *avls,enum avlcmp cmp);

void avlins(struct avls *avls,struct img *img);
void avldel(struct avls *avls,struct img *img);
char avlchk(struct avls *avls,struct img *img);

int avlnimg(struct avls *avls);
struct img *avlimg(struct avls *avls,int imgi);

#endif
