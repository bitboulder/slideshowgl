#include <stdlib.h>
#include <string.h>
#include "cfg.h"

char *cfg[]={
	"sdl.fullscreen",      "0",
	"sdl.sync",            "0",
	"dpl.efftime",         "1000",
	"dpl.displayduration", "6000",
	"dpl.shrink",          "0.75",
	NULL,
};

char **cfgfind(char *name){
	int i;
	for(i=0;cfg[i];i+=2) if(!strcmp(name,cfg[i])) return cfg+i+1;
	return NULL;
}

int cfggetint(char *name){
	char **val=cfgfind(name);
	if(val) return atoi(*val);
	return 0;
}

float cfggetfloat(char *name){
	char **val=cfgfind(name);
	if(val) return atof(*val);
	return 0.;
}

void cfgsetint(char *name, int i){
	char **val=cfgfind(name);
	*val=i?"1":"0";
}
