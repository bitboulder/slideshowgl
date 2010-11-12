#ifndef _LOAD_H
#define _LOAD_H

struct img {
	char *fn;
};

struct load {
	int nimg;
	struct img *imgs;
};
extern struct load load;

void *ldthread(void *arg);

void ldgetfiles(int argc,char **argv);

#endif
