#ifndef _ILO_H
#define _ILO_H

struct iloi {
	void *ptr;
	struct iloi *nxt;
	struct iloi *prv;
	struct iloi *hnxt;
};

struct ilo;

struct ilo *ilonew();
void ilofree(struct ilo *ilo);
void iloset(struct ilo *ilo,void *ptr);
void ilodel(struct ilo *ilo,void *ptr);
struct iloi *ilofst(struct ilo *ilo);

#endif
