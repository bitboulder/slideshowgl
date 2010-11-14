#include <stdlib.h>
#include <string.h>
#include "cfg.h"

char *cfg[]={
	"sdl.fullscreen","0",
	"sdl.sync",      "0",
	"dpl.efftime",   "1000",
	"dpl.playdelay", "5000",
	NULL,
};

int cfggetint(char *name){
	int i;
	for(i=0;cfg[i];i+=2) if(!strcmp(name,cfg[i])) return atoi(cfg[i+1]);
	return 0;
}
