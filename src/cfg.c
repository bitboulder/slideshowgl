#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include "cfg.h"
#include "main.h"
#include "sdl.h"
#include "help.h"
#include "pano.h"

enum cfgtype { CT_STR, CT_INT, CT_ENM, CT_FLT, CT_COL };
enum cfgmode { CM_INC, CM_FLIP, CM_SET };

#define E(X)	#X
#define E2(X,N)	#X
struct cfg {
	char opt;
	char *name;
	enum cfgtype type;
	enum cfgmode mode;
	char *val;
	char *vals[16];
	char *help;
} cfg[]={
	{ 'h', "cfg.usage",           CT_INT, CM_FLIP, "0", {NULL}, __("Print this usage") },
	{ 'V', "cfg.version",         CT_INT, CM_FLIP, "0", {NULL}, __("Print version") },
	{ 'v', "main.dbg",            CT_INT, CM_INC,  "0", {NULL}, __("Increase debug level") },
	{ 't', "main.timer",          CT_ENM, CM_SET,  "none", { ETIMER, NULL }, __("Activate time measurement") },
	{ 'f', "sdl.fullscreen",      CT_INT, CM_FLIP, "1", {NULL}, __("Toggle fullscreen") },
	{ 'W', "sdl.width",           CT_INT, CM_SET,  "1024", {NULL}, __("Set window width") },
	{ 'H', "sdl.height",          CT_INT, CM_SET,  "640", {NULL}, __("Set window height") },
	{ 's', "sdl.sync",            CT_INT, CM_FLIP, "1", {NULL}, __("Toggle video sync") },
	{ 0,   "sdl.hidecursor",      CT_INT, CM_SET,  "1500", {NULL}, NULL },
	{ 'd', "dpl.displayduration", CT_INT, CM_SET,  "7000", {NULL}, __("Set display duration") },
	{ 'D', "eff.efftime",         CT_INT, CM_SET,  "1000", {NULL}, __("Set effect duration") },
	{ 0,   "eff.shrink",          CT_FLT, CM_SET,  "0.75", {NULL}, NULL },
	{ 'l', "dpl.loop",            CT_INT, CM_FLIP, "0", {NULL}, __("Toggle image loop") },
	{ 'r', "ld.random",           CT_INT, CM_FLIP, "0", {NULL}, __("Toggle image random") },
	{ 0,   "ld.maxtexsize",       CT_INT, CM_SET,  "512", {NULL}, NULL },
	{ 0,   "ld.maxpanotexsize",   CT_INT, CM_SET,  "1024", {NULL}, NULL },
	{ 0,   "ld.maxpanopixels",    CT_INT, CM_SET,  "40000000", {NULL}, NULL },
	{ 0,   "gl.font",             CT_STR, CM_SET,  "FreeSans.ttf", {NULL}, NULL },
	{ 0,   "gl.inputnum_lineh",   CT_FLT, CM_SET,  "0.05", {NULL}, NULL },
	{ 0,   "gl.stat_lineh",       CT_FLT, CM_SET,  "0.025", {NULL}, NULL },
	{ 0,   "gl.txt_bgcolor",      CT_COL, CM_SET,  "0.8 0.8 0.8 0.7", {NULL}, NULL },
	{ 0,   "gl.txt_fgcolor",      CT_COL, CM_SET,  "0.0 0.0 0.0 1.0", {NULL}, NULL },
	{ 0,   "pano.defrot",         CT_FLT, CM_SET,  "0.5", {NULL}, NULL }, /* screens per second */
	{ 0,   "pano.minrot",         CT_FLT, CM_SET,  "0.125", {NULL}, NULL },
	{ 0,   "pano.texdegree",      CT_FLT, CM_SET,  "6.0", {NULL}, NULL },
	{ 0,   "pano.radius",         CT_FLT, CM_SET,  "10.0", {NULL}, NULL },
	{ 0,   "pano.fishmode",       CT_ENM, CM_SET,  "angle", {PANOFM}, NULL },
	{ 0,   "dpl.stat_rise",       CT_INT, CM_SET,  "250", {NULL}, NULL  },
	{ 0,   "dpl.stat_on",         CT_INT, CM_SET,  "5000", {NULL}, NULL },
	{ 0,   "dpl.stat_fall",       CT_INT, CM_SET,  "1000", {NULL}, NULL },
	{ 'm', "act.mark_fn",         CT_STR, CM_SET,  ".mark", {NULL}, __("Set mark file") },
	{ 0,   "act.savemarks_delay", CT_INT, CM_SET,  "10000", {NULL}, NULL },
	{ 'n', "act.do",              CT_INT, CM_FLIP, "1", {NULL}, __("Toggle actions") },
	{ 0,   NULL,                  0,      0,       NULL, {NULL}, NULL },
};
#undef E2
#undef E

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
int cfggetenum(char *name){
	int i;
	struct cfg *cfg=cfgfind(name);
	if(cfg && cfg->type==CT_ENM){
		for(i=0;cfg->vals[i];i++) if(!strcasecmp(cfg->val,cfg->vals[i])) return i;
		error(ERR_CONT,"cfg enum with unknown value '%s': '%s'",name,cfg->val);
	}else error(ERR_CONT,"cfg not of type enum '%s'",name);
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
void cfggetcol(char *name,float *col){
	struct cfg *cfg=cfgfind(name);
	if(cfg && cfg->type==CT_COL){
		if(sscanf(cfg->val,"%f %f %f %f",col+0,col+1,col+2,col+3)!=4)
			error(ERR_CONT,"cfg parse col error (%s)",cfg->val);
	}else error(ERR_CONT,"cfg not of type col '%s'",name);
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

void version(){
	printf(_("%s version %s\n"),APPNAME,VERSION);
}

void usage(char *fn){
	int i;
	version();
	printf(_("Usage: %s [Options] {FILES|FILELISTS.flst}\n"),fn);
	printf("%s:\n",_("Options"));
	for(i=0;cfg[i].name;i++) if(cfg[i].opt){
		printf("  -%c %s  %s",cfg[i].opt,cfg[i].mode==CM_FLIP?" ":"X",_(cfg[i].help?cfg[i].help:cfg[i].name));
		if(cfg[i].type==CT_ENM){
			int j;
			for(j=0;cfg[i].vals[j];j++) printf("%s%s",j?",":" [",cfg[i].vals[j]);
			printf("]");
		}
		printf(" (%s: %s)\n",_("cur"),cfg[i].mode==CM_FLIP?(atoi(cfg[i].val)?"on":"off"):cfg[i].val);
	}
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
#if HAVE_GETTEXT
	struct stat st;
	setlocale(LC_ALL,"");
	setlocale(LC_NUMERIC,"C");
	if(!stat(LOCALEDIR,&st)) bindtextdomain(APPNAME,LOCALEDIR); else if(argc){
		for(i=strlen(argv[0])-1;i>=0;i--) if(argv[0][i]=='/' || argv[0][i]=='\\') break;
		char buf[FILELEN];
		snprintf(buf,FILELEN,argv[0]);
		snprintf(buf+i+1,FILELEN-i-1,"locale");
		bindtextdomain(APPNAME,buf);
	}
	textdomain(APPNAME);
	bind_textdomain_codeset(APPNAME,"UTF-8");
#endif
	while((c=getopt(argc,argv,cfgcompileopt()))>=0)
		for(i=0;cfg[i].name;i++) if(cfg[i].opt==c) cfgset(cfg+i,optarg);
	if(cfggetint("cfg.usage")) usage(argv[0]);
	if(cfggetint("cfg.version")) version();
}

