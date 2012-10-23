#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>

#include "ilo.h"

#define H_NCH	512

struct ilo {
	struct iloi *fst;
	struct iloi *hash[H_NCH];
	SDL_mutex *mx;
};

struct ilo *ilonew(){
	struct ilo *ilo=calloc(1,sizeof(struct ilo));
	ilo->mx=SDL_CreateMutex();
	return ilo;
}

void ilofree(struct ilo *ilo){
	struct iloi *iloi=ilo->fst,*iloin;
	SDL_DestroyMutex(ilo->mx);
	for(;iloi;iloi=iloin){ iloin=iloi->nxt; free(iloi); }
	free(ilo);
}

#define iloch(ptr)	((((unsigned long)(ptr))>>6)%H_NCH)

#ifndef ILODEBUG
	#define ilochk(A)
#else
void ilochk(struct ilo *ilo){
	struct iloi *iloi=ilo->fst,*iloh;
	int ch;
	for(;iloi;iloi=iloi->nxt){
		if(iloi->nxt && iloi->nxt->prv!=iloi) abort();
		if(iloi->prv && iloi->prv->nxt!=iloi) abort();
		if(!iloi->prv && ilo->fst!=iloi) abort();
		ch=iloch(iloi->ptr);
		for(iloh=ilo->hash[ch];iloh && iloh!=iloi;) iloh=iloh->hnxt;
		if(!iloh) abort();
	}
	for(ch=0;ch<H_NCH;ch++) for(iloh=ilo->hash[ch];iloh;iloh=iloh->hnxt){
		for(iloi=ilo->fst;iloi && iloi!=iloh;) iloi=iloi->nxt;
		if(!iloi) abort();
	}
}

char ilofind(struct ilo *ilo,void *ptr){
	int ch=iloch(ptr);
	struct iloi *iloi=ilo->hash[ch];
	for(;iloi;iloi=iloi->hnxt) if(iloi->ptr==ptr) return 1;
	return 0;
}
#endif

void iloset(struct ilo *ilo,void *ptr){
	int ch=iloch(ptr);
	struct iloi *iloi;
	SDL_LockMutex(ilo->mx);
	iloi=ilo->hash[ch];
	for(;iloi;iloi=iloi->hnxt) if(iloi->ptr==ptr) goto end;
	iloi=malloc(sizeof(struct iloi));
	iloi->ptr=ptr;
	iloi->hnxt=ilo->hash[ch]; ilo->hash[ch]=iloi;
	iloi->prv=NULL; 
	iloi->nxt=ilo->fst;
	if(ilo->fst) ilo->fst->prv=iloi;
	ilo->fst=iloi;
	ilochk(ilo);
end:
	SDL_UnlockMutex(ilo->mx);
}

void ilodel(struct ilo *ilo,void *ptr){
	int ch=iloch(ptr);
	struct iloi **iloi;
	SDL_LockMutex(ilo->mx);
	iloi=ilo->hash+ch;
	for(;iloi[0];iloi=&iloi[0]->hnxt) if(iloi[0]->ptr==ptr){
		struct iloi *idel=iloi[0];
		iloi[0]=idel->hnxt;
		if(idel->prv) idel->prv->nxt=idel->nxt; else ilo->fst=idel->nxt;
		if(idel->nxt) idel->nxt->prv=idel->prv;
		free(idel);
		if(!iloi[0]) break;
	}
	ilochk(ilo);
	SDL_UnlockMutex(ilo->mx);
}

struct iloi *ilofst(struct ilo *ilo){ return ilo->fst; }

