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

#if 0
char avlprint1(struct avl *avl,const char *pre,int d,avlcmp cmp,FILE *fd){
	int i;
	char err=0;
	fprintf(fd,"[%2i]",d);
	for(i=0;i<d;i++) fprintf(fd," ");
	fprintf(fd,"%s",pre);
	if(!avl) fprintf(fd,"(--)\n"); else {
		fprintf(fd,"(%2i): %s",avl->h,imgfilefn(avl->img->file));
		if(avl->ch[0]==avl) fprintf(fd," ERROR: ch0->loop\n");
		else if(avl->ch[1]==avl) fprintf(fd," ERROR ch1->loop\n");
		else{
			int h0=avl->ch[0]?avl->ch[0]->h:0;
			int h1=avl->ch[1]?avl->ch[1]->h:0;
			int h=MAX(h0,h1)+1;
			if(avl->h!=h) fprintf(fd," ERROR: h %i",h);
			if((char)(avl->ch[0] && avl->ch[0]->pa!=avl)){ err=1; fprintf(fd," ERROR: ch0->pa"); }
			if((char)(avl->ch[1] && avl->ch[1]->pa!=avl)){ err=1; fprintf(fd," ERROR: ch1->pa"); }
			if((char)(avl->ch[0] && cmp(avl,avl->ch[0])<=0)){ err=1; fprintf(fd," ERROR: ch0->cmp"); }
			if((char)(avl->ch[1] && cmp(avl,avl->ch[1])>0)){ err=1; fprintf(fd," ERROR: ch1->cmp"); }
			if(abs(h0-h1)>1) fprintf(fd," ERROR: unbal");
			fprintf(fd,"\n");
			err|=avlprint1(avl->ch[0],"ch0",d+1,cmp,fd);
			err|=avlprint1(avl->ch[1],"ch1",d+1,cmp,fd);
		}
	}
	return err;
}

void avlprint(struct avls *avls,const char *pre,struct avl *pos){
	static struct avls *savls;
//	FILE *fd=fopen("avldebug","a");
	FILE *fd=fopen("/dev/null","w");
	char ret;
	fprintf(fd,"POS: %s\n",imgfilefn(pos->img->file));
	if(avls) savls=avls; else avls=savls;
	ret=avlprint1(avls->avl->ch[0],pre,0,avls->cmp,fd);
	fclose(fd);
	if(ret) abort();
}
#else
	#define avlprint(avls,pre,pos)
#endif

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
	avlprint(NULL,"ROT",avl);
	if(ho!=avl->h) avlrot(avl->pa);
}

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
		avlprint(avls,"INSs",avl);
		avlrot(avl->pa);
		avlprint(avls,"INSf",avl);
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
	img->nxt=img->prv=NULL;
	if(avls->cmp && img->avl && img->avl->pa){
		struct avl *avl=img->avl, *rot=avl->pa, *ins=NULL;
		int pos=avl->pa->ch[0]==avl?0:1, posi=0;
		if(!avl->ch[0]){  if((avl->pa->ch[pos]=avl->ch[1])) avl->ch[1]->pa=avl->pa; }
		else if(!avl->ch[1]){ avl->pa->ch[pos]=avl->ch[0];  avl->ch[0]->pa=avl->pa; }
		else{
			for(ins=avl->ch[0];ins->ch[1];ins=ins->ch[1]) posi=1;
			if((ins->pa->ch[posi]=ins->ch[0])) ins->ch[0]->pa=ins->pa; // del ins
			rot=ins->pa;
			avl->pa->ch[pos]=ins;       ins->pa=avl->pa;     // ins => pa->ch[pos]
			if((ins->ch[0]=avl->ch[0])) ins->ch[0]->pa=ins;  // avl->ch[0] => ins->ch[0]
			if((ins->ch[1]=avl->ch[1])) ins->ch[1]->pa=ins;  // avl->ch[1] => ins->ch[1]
			ins->h=avl->h;
		}
		avlprint(avls,"DELs",avl);
		avlrot(rot);
		avlprint(avls,"DELm",avl);
		if(ins) avlrot(ins);
		avlprint(avls,"DELf",avl);
		img->avl=NULL;
		free(avl);
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
