#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "avl.h"
#include "file.h"
#include "exif.h"
#include "help.h"

struct avl {
	int rnd;
	struct img *img;
	struct avl *pa;
	struct avl *ch[2];
	int h;
};
struct avls {
	avlcmp cmp;
	struct avl *avl;
	struct img **first,**last;
};

#define AVLDIRCMP(a,b)	{ \
	const char *ar=imgfiledir(a->img->file); \
	const char *br=imgfiledir(b->img->file); \
	if(ar && !br) return  1; \
	if(!ar && br) return -1; \
}

int avlrndcmp(const struct avl *a,const struct avl *b){
	AVLDIRCMP(a,b);
	if(a->rnd>b->rnd) return 1;
	if(a->rnd<b->rnd) return -1;
	return a->img<b->img?-1:1;
}

int avlfilecmp(const struct avl *a,const struct avl *b){
	int str;
	AVLDIRCMP(a,b);
	str=strcmp(imgfilefn(a->img->file),imgfilefn(b->img->file));
	return str ? str : avlrndcmp(a,b);
}

int avldatecmp(const struct avl *a,const struct avl *b){
	int64_t ad,bd;
	AVLDIRCMP(a,b);
	ad=imgexifdate(a->img->exif);
	bd=imgexifdate(b->img->exif);
	if(!ad && !bd) return avlfilecmp(a,b);
	if(!ad)   return  1;
	if(!bd)   return -1;
	if(ad<bd) return -1;
	if(ad>bd) return  1;
	return avlfilecmp(a,b);
}

char avlprint1(struct avl *avl,const char *pre,int d,avlcmp cmp){
	int i;
	char err=0;
	printf("[%2i]",d);
	for(i=0;i<d;i++) printf(" ");
	printf("%s",pre);
	if(!avl) printf("(--)\n"); else {
		printf("(%2i): %s",avl->h,imgfilefn(avl->img->file));
		if(avl->ch[0]==avl) printf(" ERROR: ch0->loop\n");
		else if(avl->ch[1]==avl) printf(" ERROR ch1->loop\n");
		else{
			int h0=avl->ch[0]?avl->ch[0]->h:0;
			int h1=avl->ch[1]?avl->ch[1]->h:0;
			int h=MAX(h0,h1)+1;
			if(avl->h!=h) printf(" ERROR: h %i",h);
			if((char)(avl->ch[0] && avl->ch[0]->pa!=avl)){ err=1; printf(" ERROR: ch0->pa"); }
			if((char)(avl->ch[1] && avl->ch[1]->pa!=avl)){ err=1; printf(" ERROR: ch1->pa"); }
			if((char)(avl->ch[0] && cmp(avl,avl->ch[0])<=0)){ err=1; printf(" ERROR: ch0->cmp"); }
			if((char)(avl->ch[1] && cmp(avl,avl->ch[1])>0)){ err=1; printf(" ERROR: ch1->cmp"); }
			if(abs(h0-h1)>1) printf(" ERROR: unbal");
			printf("\n");
			err|=avlprint1(avl->ch[0],"ch0",d+1,cmp);
			err|=avlprint1(avl->ch[1],"ch1",d+1,cmp);
		}
	}
	return err;
}

char avlprint(struct avls *avls,const char *pre){
	static struct avls *savls;
	if(avls) savls=avls; else avls=savls;
	return avlprint1(avls->avl->ch[0],pre,0,avls->cmp);
}

void avlh(struct avl *avl){ avl->h=MAX(avl->ch[0]?avl->ch[0]->h:0,avl->ch[1]?avl->ch[1]->h:0)+1; }

struct avl *avlrot1(struct avl *avl,int dir){
	struct avl *rot=avl->ch[dir];
	if((rot->ch[dir]?rot->ch[dir]->h:0) < rot->h-1){ /* doppel */
		struct avl *sub=rot->ch[!dir];
		avl->pa->ch[avl->pa->ch[0]==avl?0:1]=sub; sub->pa=avl->pa;          // sub => pa->ch
		if((rot->ch[!dir]=sub->ch[dir]))          rot->ch[!dir]->pa=rot;    // sub->ch[dir] => rot->ch[!dir]
		sub->ch[dir]=rot;                         rot->pa=sub;              // rot => sub->ch[dir]
		if((avl->ch[dir]=sub->ch[!dir]))          avl->ch[dir]->pa=avl;     // sub->ch[!dir] => avl->ch[dir]
		sub->ch[!dir]=avl;                        avl->pa=sub;              // avl => sub->ch[!dir]
		avlh(avl); avlh(rot); avlh(sub);
		return sub;
	}else{ /* einfach */
		avl->pa->ch[avl->pa->ch[0]==avl?0:1]=rot; rot->pa=avl->pa;          // rot => pa->ch
		if((avl->ch[dir]=rot->ch[!dir]))          avl->ch[dir]->pa=avl;     // rot->ch[!dir] => avl->ch[dir]
		rot->ch[!dir]=avl;                        avl->pa=rot;              // avl => rot->ch[!dir]
		avlh(avl); avlh(rot);
		return rot;
	}
}

void avlrot(struct avl *avl){
	int ho,h0,h1;
	if(!avl || !avl->img) return;
	ho=avl->h;
	h0=avl->ch[0]?avl->ch[0]->h:0;
	h1=avl->ch[1]?avl->ch[1]->h:0;
	avl->h=MAX(h0,h1)+1;
	if(h0<h1-1) avl=avlrot1(avl,1);
	if(h1<h0-1) avl=avlrot1(avl,0);
//	printf("%s\n",imgfilefn(avl->img->file)); if(avlprint(NULL,"ROT")) exit(1);
	if(ho!=avl->h) avlrot(avl->pa);
}

void avlout1(struct avl *avl,struct img **first,struct img **last,struct img *img,char pos){
	if(!avl) return;
	if(!img){
		avl->img->nxt=avl->img->prv=NULL;
		*first=*last=avl->img;
	}else if(!pos){
		avl->img->nxt=img;
		avl->img->prv=img->prv;
		if(img->prv) img->prv->nxt=avl->img; else *first=avl->img;
		img->prv=avl->img;
	}else{
		avl->img->prv=img;
		avl->img->nxt=img->nxt;
		if(img->nxt) img->nxt->prv=avl->img; else *last=avl->img;
		img->nxt=avl->img;
	}
	avlout1(avl->ch[0],first,last,avl->img,0);
	avlout1(avl->ch[1],first,last,avl->img,1);
}

void avlout(struct avls *avls){ avlout1(avls->avl->ch[0],avls->first,avls->last,NULL,0); }

void avlins(struct avls *avls,struct img *img){
	if(avls->cmp){
		struct avl **p=avls->avl->ch+0, *pa=avls->avl;
		struct avl *avl=calloc(1,sizeof(struct avl));
		int pos=-1;
		avl->rnd=rand();
		avl->img=img;
		img->avl=avl;
		while(*p){ pa=*p; p=p[0]->ch+(pos=(avls->cmp(avl,p[0])>0?1:0)); }
		*p=avl;
		avl->pa=pa;
		avl->h=1;
		if(pos<0){
			img->nxt=img->prv=NULL;
			*avls->first=*avls->last=img;
		}else if(!pos){
			img->nxt=pa->img;
			img->prv=pa->img->prv;
			pa->img->prv=img;
			if(!img->prv) *avls->first=img;
			else img->prv->nxt=img;
		}else{
			img->prv=pa->img;
			img->nxt=pa->img->nxt;
			pa->img->nxt=img;
			if(!img->nxt) *avls->last=img;
			else img->nxt->prv=img;
		}
//		printf("%s\n",imgfilefn(img->file)); if(avlprint(avls,"INS")) exit(1);
		avlrot(avl->pa);
	}else{
		img->nxt=NULL;
		img->prv=avls->last[0];
		if(avls->last[0]) avls->last[0]->nxt=img;
		else avls->first[0]=img;
		avls->last[0]=img;
		img->avl=NULL;
	}
}

void avldel(struct avls *avls,struct img *img){
	if(img->prv) img->prv->nxt=img->nxt; else avls->first[0]=img->nxt;
	if(img->nxt) img->nxt->prv=img->prv; else avls->last[0]=img->prv;
	if(avls->cmp){
		struct avl *p=img->avl;
		int pos=0;
		if(p->ch[0]) while(p->ch[0]) p=p->ch[0];
		else pos=p->pa->ch[0]==p?0:1;
		p->pa->ch[pos]=p->ch[1];
		if(p->ch[1]) p->ch[1]->pa=p->pa;
		avlrot(p->pa);
	}
}

char avlchk(struct avls *avls,struct img *img){
	if(!avls->cmp) return 0;
	if((!img->prv || avls->cmp(img->prv->avl,img->avl)<=0) &&
	   (!img->nxt || avls->cmp(img->avl,img->nxt->avl)<=0)) return 0;
	avldel(avls,img);
	avlins(avls,img);
	return 1;
}

struct avls *avlinit(avlcmp cmp,struct img **first,struct img **last){
	struct avls *avls=calloc(1,sizeof(struct avls));
	avls->avl=calloc(1,sizeof(struct avl));
	avls->cmp=cmp;
	avls->first=first;
	avls->last=last;
	return avls;
}

void avlfree1(struct avl *avl){
	if(!avl) return;
	if(avl->img) avl->img->avl=NULL;
	avlfree1(avl->ch[0]);
	avlfree1(avl->ch[1]);
	free(avl);
}

void avlfree(struct avls *avls){
	avlfree1(avls->avl);
	free(avls);
}
