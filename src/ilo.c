#include <stdio.h>
#include <stdlib.h>

#include "ilo.h"

#define H_NCH	512

struct ilo {
	struct iloi *fst;
	struct iloi *hash[H_NCH];
};

struct ilo *ilonew(){
	return calloc(1,sizeof(struct ilo));
}

void ilofree(struct ilo *ilo){
	struct iloi *iloi=ilo->fst;
	for(;iloi;iloi=iloi->nxt) free(iloi);
	free(ilo);
}

#define iloch(ptr)	((((unsigned long)(ptr))>>6)%H_NCH)

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

void iloset(struct ilo *ilo,void *ptr){
#if 0
	int ch=iloch(ptr);
	struct iloi *iloi=ilo->hash[ch];
	for(;iloi;iloi=iloi->hnxt) if(iloi->ptr==ptr) return;
	iloi=malloc(sizeof(struct iloi));
	iloi->ptr=ptr;
	iloi->hnxt=ilo->hash[ch]; ilo->hash[ch]=iloi;
	iloi->prv=NULL; 
	iloi->nxt=ilo->fst;
	if(ilo->fst) ilo->fst->prv=iloi;
	ilo->fst=iloi;
	ilochk(ilo);
#endif
}

void ilodel(struct ilo *ilo,void *ptr){
#if 0
	int ch=iloch(ptr);
	struct iloi **iloi=ilo->hash+ch;
	for(;iloi[0];iloi[0]=iloi[0]->hnxt) if(iloi[0]->ptr==ptr){
		struct iloi *idel=iloi[0];
		iloi[0]=idel->hnxt;
		if(idel->prv) idel->prv->nxt=idel->nxt; else ilo->fst=idel->nxt;
		if(idel->nxt) idel->nxt->prv=idel->prv;
		free(idel);
		if(!iloi[0]) break;
	}
	ilochk(ilo);
#endif
}

struct iloi *ilofst(struct ilo *ilo){ return ilo->fst; }

