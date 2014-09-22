#ifndef _ZLIB_H
#define _ZLIB_H

#if HAVE_ZLIB
	#include <zlib.h>
#else
	#define gzFile			FILE*
	#define gzopen			fopen
	#define	gzclose			fclose
	#define gzeof			feof
	#define gzprintf		fprintf
	#define gzgets(f,b,l)	fgets(b,l,f)
#endif

#endif
