#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "SDL.h"

void error(char quit,char *txt,...){
	int lfmt=strlen(txt)+9;
	char *fmt=malloc(lfmt);
	va_list ap;
	snprintf(fmt,lfmt,"ERROR: %s\n",txt);
	va_start(ap,txt);
	vfprintf(stderr,fmt,ap);
	va_end(ap);
	free(fmt);
	if(quit) exit(1);
}

int main(int argc,char **argv){
	if(SDL_Init(SDL_INIT_TIMER)<0) error(1,"sdl init failed");
	return 0;
}
