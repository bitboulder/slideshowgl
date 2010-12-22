#ifndef _LDCP_H
#define _LDCP_H

#include "img.h"

struct loadconcept {
	const char *load_str;
	const char *hold_str;
	struct loadconept_ld {
		int imgi;
		enum imgtex tex;
	} *load;
	enum imgtex *hold;
	int hold_min;
	int hold_max;
};

void ldconceptcompile();
void ldconceptfree();
struct loadconcept *ldconceptget();

#endif
