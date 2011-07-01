#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include "main.h"
#include "cfg.h"
#include "sdl.h"
#include "help.h"
#include "pano.h"
#include "mark.h"

enum cfgtype { CT_STR, CT_INT, CT_ENM, CT_FLT, CT_COL };

#define CM	E2(INC,0),E2(FLIP,0),E2(SET,1),E2(DO,1)
#define E2(X,A)	CM_##X
enum cfgmode { CM, CM_NUM };
#undef E2
#define E2(X,A)	A
char cfgmodearg[CM_NUM]={CM};
#undef E2

#define E(X)	#X
#define E2(X,N)	#X
struct cfg {
	char opt;
	const char *name;
	enum cfgtype type;
	enum cfgmode mode;
	const char *val;
	const char *vals[16];
	const char *help;
} cfgs[]={
	{ 'h', "cfg.usage",           CT_INT, CM_INC,  "0",      {NULL}, __("Print this usage") },
	{ 'V', "cfg.version",         CT_INT, CM_FLIP, "0",      {NULL}, __("Print version") },
	{ 'v', "main.dbg",            CT_INT, CM_INC,  "0",      {NULL}, __("Increase debug level") },
	{ 't', "main.timer",          CT_ENM, CM_SET,  "none",   {ETIMER,NULL}, __("Activate time measurement") },
	{ 'f', "sdl.fullscreen",      CT_INT, CM_FLIP, "1",      {NULL}, __("Toggle fullscreen") },
	{ 0,   "sdl.display",         CT_INT, CM_SET,  "1",      {NULL}, __("Set display id to use in multihead mode (overwritten by SDL_VIDEO_FULLSCREEN_DISPLAY)") },
	{ 'W', "sdl.width",           CT_INT, CM_SET,  "1024",   {NULL}, __("Set window width") },
	{ 'H', "sdl.height",          CT_INT, CM_SET,  "768",    {NULL}, __("Set window height") },
	{ 's', "sdl.sync",            CT_INT, CM_FLIP, "1",      {NULL}, __("Toggle video sync") },
	{ 0,   "sdl.hidecursor",      CT_INT, CM_SET,  "1500",   {NULL}, __("After that time (ms) the cursor is hidden") },
	{ 0,   "sdl.doubleclicktime", CT_INT, CM_SET,  "500",    {NULL}, __("Delay for double-click in ms") },
	{ 0,   "sdl.fsaa",            CT_INT, CM_SET,  "4",      {NULL}, __("Antialias mode") },
	{ 0,   "sdl.fsaa_max_w",      CT_INT, CM_SET,  "1440",   {NULL}, __("Maximal window width for antialias") },
	{ 0,   "sdl.fsaa_max_h",      CT_INT, CM_SET,  "1024",   {NULL}, __("Maximal window height for antialias") },
	{ 'd', "dpl.displayduration", CT_INT, CM_SET,  "6000",   {NULL}, __("Set display duration") },
	{ 'l', "dpl.loop",            CT_INT, CM_FLIP, "0",      {NULL}, __("Toggle image loop") },
	{ 'z', "dpl.initzoom",        CT_INT, CM_SET,  "0",      {NULL}, __("Set initial zoom") },
	{ 'g', "dpl.playmode",        CT_INT, CM_FLIP, "0",      {NULL}, __("Play all and exit") },
	{ 'G', "sdpl.playrecord",     CT_STR, CM_SET,  "",       {NULL}, __("Enable playrecord + set file name base") },
	{ 0,   "dpl.playrecord_rate", CT_INT, CM_SET,  "25",     {NULL}, __("Framerate for playrecord") },
	{ 0,   "dpl.infosel",         CT_INT, CM_SET,  "241",    {NULL}, __("Selected Exif-Info (Bitmask)") },
	{ 0,   "sdl.playrecord_w",    CT_INT, CM_SET,  "720",    {NULL}, __("Window width for playrecord") },
	{ 0,   "sdl.playrecord_h",    CT_INT, CM_SET,  "576",    {NULL}, __("Window height for playrecord") },
	{ 'D', "eff.efftime",         CT_INT, CM_SET,  "1000",   {NULL}, __("Set effect duration") },
	{ 0,   "eff.shrink",          CT_FLT, CM_SET,  "0.75",   {NULL}, __("Factor to shrink all not-current images for zoom<0") },
	{ 'r', "il.random",           CT_INT, CM_FLIP, "0",      {NULL}, __("Toggle image random") },
	{ 'e', "il.datesort",         CT_INT, CM_FLIP, "1",      {NULL}, __("Toggle sort by exif date") },
	{ 'E', "il.datesortdir",      CT_INT, CM_FLIP, "1",      {NULL}, __("Toggle sort by exif date of subdirectories") },
	{ 0,   "il.maxhistory",       CT_INT, CM_SET,  "200",    {NULL}, __("Maximal size of imglist history (for loop protection)") },
	{ 0,   "ld.maxtexsize",       CT_INT, CM_SET,  "512",    {NULL}, __("Maximal texture size") },
	{ 0,   "ld.maxpanotexsize",   CT_INT, CM_SET,  "1024",   {NULL}, __("Maximal texture size for panoramas") },
	{ 0,   "ld.maxpanopixels",    CT_INT, CM_SET,  "60000000",     {NULL}, __("Maximal pixels for panoramas (bigger ones are scaled down)") },
	{ 0,   "ld.filetime_check",   CT_INT, CM_SET,  "5000",   {NULL}, __("File time check delay") },
	{ 0,   "ld.numexifloadperimg",CT_INT, CM_SET,  "5",      {NULL}, __("Number of exif-file-loads per one img-file-load") },
	{ 0,   "img.holdfolders",     CT_INT, CM_SET,  "3",      {NULL}, __("Number of directories to hold") },
	{ 0,   "gl.font",             CT_STR, CM_SET,  "FreeSans.ttf", {NULL}, __("Filename of the font to use") },
	{ 0,   "gl.hrat_input",       CT_FLT, CM_SET,  "0.05",   {NULL}, __("Textheight for input") },
	{ 0,   "gl.hrat_stat",        CT_FLT, CM_SET,  "0.025",  {NULL}, __("Textheight for statusbar") },
	{ 0,   "gl.hrat_txtimg",      CT_FLT, CM_SET,  "0.1",    {NULL}, __("Textheight for text-images") },
	{ 0,   "gl.hrat_dirname",     CT_FLT, CM_SET,  "0.15",   {NULL}, __("Textheight for directory names") },
	{ 0,   "gl.hrat_cat",         CT_FLT, CM_SET,  "0.03",   {NULL}, __("Textheight for catalog names") },
	{ 0,   "gl.txt_border",       CT_FLT, CM_SET,  "0.1",    {NULL}, __("Border around text (in percent of height)") },
	{ 0,   "gl.dir_border",       CT_FLT, CM_SET,  "0.1",    {NULL}, __("Border around dirname (in percent of height)") },
	{ 0,   "gl.col_txtbg",        CT_COL, CM_SET,  "0.8 0.8 0.8 0.7", {NULL}, __("Text background color") },
	{ 0,   "gl.col_txtfg",        CT_COL, CM_SET,  "0.0 0.0 0.0 1.0", {NULL}, __("Text foreground color") },
	{ 0,   "gl.col_txtmk",        CT_COL, CM_SET,  "1.0 0.6 0.0 1.0", {NULL}, __("Text mark color") },
	{ 0,   "gl.col_colmk",        CT_COL, CM_SET,  "0.6 1.0 0.0 1.0", {NULL}, __("Text mark color") },
	{ 0,   "gl.col_dirname",      CT_COL, CM_SET,  "1.0 1.0 1.0 1.0", {NULL}, __("Text dirname color") },
	{ 0,   "gl.col_playicon",     CT_COL, CM_SET,  "0.0 0.0 0.0 0.7", {NULL}, __("Text play icon color") },
	{ 0,   "pano.defrot",         CT_FLT, CM_SET,  "0.125",  {NULL}, __("Default rotation speed for panoramas (screens per second)") },
	{ 0,   "pano.minrot",         CT_FLT, CM_SET,  "0.0625", {NULL}, __("Minimal rotation speed for panoramas") },
	{ 0,   "pano.texdegree",      CT_FLT, CM_SET,  "6.0",    {NULL}, __("Maximal texture size for panoramas (in degree)") },
	{ 'p', "pano.mintexsize",     CT_INT, CM_SET,  "16",     {NULL}, __("Set minimal texture size for panorama mode") },
	{ 0,   "pano.radius",         CT_FLT, CM_SET,  "10.0",   {NULL}, __("Radius of sphere for panoramas") },
	{ 0,   "pano.fishmode",       CT_ENM, CM_SET,  "angle",  {PANOFM}, __("Panorama fisheye mode") },
	{ 0,   "pano.maxfishangle",   CT_FLT, CM_SET,  "300.",   {NULL}, __("Maximal angle for isogonic panorama fisheye mode") },
	{ 0,   "eff.stat_rise",       CT_INT, CM_SET,  "250",    {NULL}, __("Rise time for statusbar (ms)") },
	{ 0,   "eff.stat_on",         CT_INT, CM_SET,  "5000",   {NULL}, __("Time the statusbar remains (ms)") },
	{ 0,   "eff.stat_fall",       CT_INT, CM_SET,  "1000",   {NULL}, __("Fall time for statusbar (ms)") },
	{ 0,   "eff.wnd_delay",       CT_INT, CM_SET,  "1000",   {NULL}, __("Rise and fall time for windows (ms)") },
	{ 0,   "eff.txt_delay",       CT_INT, CM_SET,  "300",    {NULL}, __("Rise and fall time for text windows (ms)") },
	{ 'm', "mark.fn",             CT_STR, CM_SET,  "",       {NULL}, __("Set mark file (Default: $TEMP/slideshowgl-mark)") },
	{ 'k', "mark.catalog",        CT_STR, CM_DO,   "",       {NULL}, __("Add catalog (directory also possible)") },
	{ 0,   "mark.flst2gthumb",    CT_STR, CM_SET,  "/mnt/img/self/scriptz/flst2gthumb.pl", {NULL}, __("Skript for flst conversion") },
	{ 0,   "act.savemarks_delay", CT_INT, CM_SET,  "5000",   {NULL}, __("Delay after that the mark file is written") },
	{ 0,   "act.ilcleanup_delay", CT_INT, CM_SET,  "3000",   {NULL}, __("Delay after that the img lists are cleaned") },
	{ 'n', "act.do",              CT_INT, CM_FLIP, "1",      {NULL}, __("Toggle actions") },
	{ 0,   "prged.w",             CT_FLT, CM_SET,  ".2",     {NULL}, __("Width of index il in prg edit mode") },
	{ 0,   "prg.rat",             CT_FLT, CM_SET,  "1.3333333",{NULL}, __("Frame dimension rate for prg") },
	{ 0,   NULL,                  0,      0,       NULL,     {NULL}, NULL },
};
#undef E2
#undef E

/* thread: all */
struct cfg *cfgfind(const char *name){
	int i;
	for(i=0;cfgs[i].name;i++) if(!strcmp(name,cfgs[i].name)) return cfgs+i;
	error(ERR_CONT,"cfg not found '%s'",name);
	return NULL;
}

/* thread: all */
int cfggetint(const char *name){
	struct cfg *cfg=cfgfind(name);
	if(cfg && cfg->type==CT_INT) return atoi(cfg->val);
	error(ERR_CONT,"cfg not of type int '%s'",name);
	return 0;
}

/* thread: all */
unsigned int cfggetuint(const char *name){ return (unsigned int)cfggetint(name); }
char cfggetbool(const char *name){ return (char)cfggetint(name); }

/* thread: all */
int cfggetenum(const char *name){
	int i;
	struct cfg *cfg=cfgfind(name);
	if(cfg && cfg->type==CT_ENM){
		for(i=0;cfg->vals[i];i++) if(!strcasecmp(cfg->val,cfg->vals[i])) return i;
		error(ERR_CONT,"cfg enum with unknown value '%s': '%s'",name,cfg->val);
	}else error(ERR_CONT,"cfg not of type enum '%s'",name);
	return 0;
}

/* thread: all */
float cfggetfloat(const char *name){
	struct cfg *cfg=cfgfind(name);
	if(cfg && cfg->type==CT_FLT) return (float)atof(cfg->val);
	error(ERR_CONT,"cfg not of type float '%s'",name);
	return 0.;
}

/* thread: all */
void cfggetcol(const char *name,float *col){
	struct cfg *cfg=cfgfind(name);
	if(cfg && cfg->type==CT_COL){
		if(sscanf(cfg->val,"%f %f %f %f",col+0,col+1,col+2,col+3)!=4)
			error(ERR_CONT,"cfg parse col error (%s)",cfg->val);
	}else error(ERR_CONT,"cfg not of type col '%s'",name);
}

/* thread: all */
const char *cfggetstr(const char *name){
	struct cfg *cfg=cfgfind(name);
	if(cfg && cfg->type==CT_STR) return cfg->val;
	error(ERR_CONT,"cfg not of type str '%s'",name);
	return NULL;
}

void cfgset(struct cfg *cfg, char *val){
	int ival;
	char *tmp;
	if(!val && cfgmodearg[cfg->mode]) error(ERR_QUIT,"cfgset no arg for '%s'",cfg->name);
	switch(cfg->mode){
	case CM_INC: case CM_FLIP:
		if(cfg->type!=CT_INT)
			error(ERR_QUIT,"cfgset unsupported mode for '%s'",cfg->name);
		ival=atoi(cfg->val);
		if(cfg->mode==CM_FLIP) ival=!ival; else ival++;
		cfg->val=tmp=malloc(10);
		snprintf(tmp,10,"%i",ival);
	break;
	case CM_SET:
		cfg->val=val;
	break;
	case CM_DO:
		if(!strcmp(cfg->name,"mark.catalog")) markcatadd(val);
	break;
	default: break;
	}
}

void version(){
	mprintf(_("%s version %s\n"),APPNAME,VERSION);
}

void usage(char *fn,char extended) NORETURN;
void usage(char *fn,char extended){
	int i;
	version();
	mprintf(_("Usage: %s [Options] {FILES|FILELISTS.flst}\n"),fn);
	mprintf("%s:\n",_("Options"));
	for(i=0;cfgs[i].name;i++) if(extended || cfgs[i].opt){
		char noarg = !cfgmodearg[cfgs[i].mode];
		mprintf("  ");
		if(cfgs[i].opt) mprintf("-%c %c",cfgs[i].opt,noarg?' ':'X'); else mprintf("    ");
		if(extended){
			char buf[64];
			snprintf(buf,64,"%s%s",cfgs[i].name,noarg?"  ":"=X");
			mprintf("%s -c %-20s",cfgs[i].opt?" /":"  ",buf);
		}
		mprintf(" %s",_(cfgs[i].help?cfgs[i].help:cfgs[i].name));
		if(cfgs[i].type==CT_ENM){
			int j;
			for(j=0;cfgs[i].vals[j];j++) mprintf("%s%s",j?",":" [",cfgs[i].vals[j]);
			mprintf("]");
		}
		mprintf(" (%s: %s)\n",_("cur"),cfgs[i].mode==CM_FLIP?(atoi(cfgs[i].val)?"on":"off"):cfgs[i].val);
	}
	exit(0);
}

char *cfgcompileopt(){
	static char opt[256];
	int i,p=2;
	opt[0]='c'; opt[1]=':';
	for(i=0;cfgs[i].name;i++) if(cfgs[i].opt){
		if(p==254) error(ERR_QUIT,"cfgcompileopt: opt too small");
		opt[p++]=cfgs[i].opt;
		if(cfgmodearg[cfgs[i].mode]) opt[p++]=':';
	}
	opt[p++]='\0';
	return opt;
}

void cfgopt(int c,char *val){
	size_t i;
	if(c=='c'){
		char *pos=strchr(val,'=');
		if(pos) *(pos++)='\0';
		for(i=0;cfgs[i].name;i++) if(!strcmp(cfgs[i].name,val)){
			char noarg = !cfgmodearg[cfgs[i].mode];
			if(!noarg && !pos) error(ERR_QUIT,"option missing argument: -c %s",val);
			cfgset(cfgs+i,pos);
			break;
		}
		if(!cfgs[i].name) error(ERR_QUIT,"unknown option: -c %s",val);
	}else
		for(i=0;cfgs[i].name;i++) if(cfgs[i].opt==c) cfgset(cfgs+i,val);
}

void cfgparseargs(int argc,char **argv){
	int c;
#if HAVE_GETTEXT
	size_t i;
	struct stat st;
	setlocale(LC_ALL,"");
	setlocale(LC_NUMERIC,"C");
	if(!stat(LOCALEDIR,&st)) bindtextdomain(APPNAME,LOCALEDIR); else if(argc){
		char buf[FILELEN];
		for(i=strlen(argv[0]);i--;) if(argv[0][i]=='/' || argv[0][i]=='\\') break;
		snprintf(buf,FILELEN,argv[0]);
		snprintf(buf+i+1,FILELEN-i-1,"locale");
		bindtextdomain(APPNAME,buf);
	}
	textdomain(APPNAME);
	bind_textdomain_codeset(APPNAME,"UTF-8");
#endif
	while((c=getopt(argc,argv,cfgcompileopt()))>=0) cfgopt(c,optarg);
	if(cfggetint("cfg.usage")) usage(argv[0],cfggetint("cfg.usage")>1);
	if(cfggetint("cfg.version")) version();
}

