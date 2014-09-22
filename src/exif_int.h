#ifndef _EXIF_INT_H
#define _EXIF_INT_H

#include "exif.h"

struct imgexif {
	char load;
	enum rot rot;
	int64_t date;
	char *info;
	struct imglist *sortil;
};

#endif
