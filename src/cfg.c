#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "cfg.h"
#include "main.h"

enum cfgtype { CT_STR, CT_INT, CT_FLT };
enum cfgmode { CM_INC, CM_FLIP, CM_SET };
char *cfgmodestr[] = { "increase", "toggle", "set" };

struct cfg {
	char opt;
	char *name;
	enum cfgtype type;
	enum cfgmode mode;
	char *val;
} cfg[]={
	{ 'h', "cfg.usage",           CT_INT, CM_FLIP, "0" },
	{ 'v', "main.dbg",            CT_INT, CM_INC,  "0" },
	{ 'f', "sdl.fullscreen",      CT_INT, CM_FLIP, "0" },
	{ 's', "sdl.sync",            CT_INT, CM_FLIP, "0" },
	{ 0,   "sdl.hidecursor",      CT_INT, CM_SET,  "1500" },
	{ 'd', "dpl.displayduration", CT_INT, CM_SET,  "7000" },
	{ 'D', "dpl.efftime",         CT_INT, CM_SET,  "1000" },
	{ 0,   "dpl.shrink",          CT_FLT, CM_SET,  "0.75" },
	{ 'l', "dpl.loop",            CT_INT, CM_FLIP, "0" },
	{ 'r', "ld.random",           CT_INT, CM_FLIP, "0" },
	{ 0,   "gl.font",             CT_STR, CM_SET,  "data/FreeSans.ttf" },
	{ 0,   "dpl.stat_rise",       CT_INT, CM_SET,  "250"  },
	{ 0,   "dpl.stat_on",         CT_INT, CM_SET,  "5000" },
	{ 0,   "dpl.stat_fall",       CT_INT, CM_SET,  "1000" },
	{ 0,   NULL,                  0,      0,       NULL },
};

/* thread: all */
struct cfg *cfgfind(char *name){
	int i;
	for(i=0;cfg[i].name;i++) if(!strcmp(name,cfg[i].name)) return cfg+i;
	error(ERR_CONT,"cfg not found '%s'",name);
	return NULL;
}

/* thread: all */
int cfggetint(char *name){
	struct cfg *cfg=cfgfind(name);
	if(cfg && cfg->type==CT_INT) return atoi(cfg->val);
	error(ERR_CONT,"cfg not of type int '%s'",name);
	return 0;
}

/* thread: all */
float cfggetfloat(char *name){
	struct cfg *cfg=cfgfind(name);
	if(cfg && cfg->type==CT_FLT) return atof(cfg->val);
	error(ERR_CONT,"cfg not of type float '%s'",name);
	return 0.;
}

/* thread: all */
char *cfggetstr(char *name){
	struct cfg *cfg=cfgfind(name);
	if(cfg && cfg->type==CT_STR) return cfg->val;
	error(ERR_CONT,"cfg not of type str '%s'",name);
	return NULL;
}

void cfgset(struct cfg *cfg, char *val){
	int ival;
	switch(cfg->mode){
	case CM_INC: case CM_FLIP:
		if(cfg->type!=CT_INT)
			error(ERR_QUIT,"cfgset unsupported mode for '%s'",cfg->name);
		ival=atoi(cfg->val);
		if(cfg->mode==CM_FLIP) ival=!ival; else ival++;
		cfg->val=malloc(10);
		snprintf(cfg->val,10,"%i",ival);
	break;
	case CM_SET:
		if(!val) error(ERR_QUIT,"cfgset no arg for CM_SET '%s'",cfg->name);
		cfg->val=val;
	break;
	}
}

void usage(char *fn){
	int i;
	printf("Usage: %s [Options] {FILES|FILELISTS.flst}\n",fn);
	printf("Options:\n");
	for(i=0;cfg[i].name;i++) if(cfg[i].opt)
		printf("  -%c  %s %s\n",
			cfg[i].opt,
			cfgmodestr[cfg[i].mode],
			cfg[i].name);
	exit(0);
}

char *cfgcompileopt(){
	static char opt[256];
	int i,p=0;
	for(i=0;cfg[i].name;i++) if(cfg[i].opt){
		if(p==254) error(ERR_QUIT,"cfgcompileopt: opt too small");
		opt[p++]=cfg[i].opt;
		if(cfg[i].mode==CM_SET) opt[p++]=':';
	}
	opt[p++]='\0';
	return opt;
}

void cfgparseargs(int argc,char **argv){
	int c,i;
	while((c=getopt(argc,argv,cfgcompileopt()))>=0)
		for(i=0;cfg[i].name;i++) if(cfg[i].opt==c) cfgset(cfg+i,optarg);
	if(cfggetint("cfg.usage")) usage(argv[0]);
}

