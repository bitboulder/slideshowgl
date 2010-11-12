#ifndef _CONFIG_H
#define _CONFIG_H

#ifdef __linux__
	#include <unistd.h>
	#define SLEEP(X)	usleep((X)*1000L)
#endif

#endif
