#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#if HAVE_REALPATH
	#include <sys/param.h>
	#include <unistd.h>
#endif
#include "file.h"
#include "main.h"
#include "img.h"
#include "cfg.h"
#include "act.h"
#include "pano.h"

/***************************** imgfile ******************************************/

struct imgfile {
	char fn[FILELEN];
	char tfn[FILELEN];
};

struct imgfile *imgfileinit(){ return calloc(1,sizeof(struct imgfile)); }
void imgfilefree(struct imgfile *ifl){ free(ifl); }

/* thread: dpl, load */
char *imgfilefn(struct imgfile *ifl){ return ifl->fn; }

/* thread: load */
char imgfiletfn(struct imgfile *ifl,char **tfn){
	if(!ifl->tfn[0]) return 0;
	*tfn=ifl->tfn;
	return 1;
}

/***************************** find *******************************************/

char findfilesubdir1(char *dst,int len,char *subdir,char *ext){
	int i;
	FILE *fd;
	char fn[FILELEN];
	char *extpos = ext ? strrchr(dst,'.') : NULL;
	if(extpos>dst+4 && !strncmp(extpos-4,"_cut",4)) extpos-=4;
	if(extpos>dst+6 && !strncmp(extpos-6,"_small",6)) extpos-=6;
	if(extpos) extpos[0]='\0';
	for(i=strlen(dst)-1;i>=0;i--) if(dst[i]=='/' || dst[i]=='\\'){
		char dsti=dst[i];
		dst[i]='\0';
		snprintf(fn,FILELEN,"%s/%s/%s%s",dst,subdir,dst+i+1,ext?ext:"");
		dst[i]=dsti;
		if((fd=fopen(fn,"rb"))){
			fclose(fd);
			strncpy(dst,fn,len);
			return 1;
		}
		if(subdir[0]=='\0') break;
	}
	if(extpos) extpos[0]='.';
	return 0;
}

char findfilesubdir(char *dst,char *subdir,char *ext){
	if(findfilesubdir1(dst,FILELEN,subdir,ext)) return 1;
#if HAVE_REALPATH
	{
		static char rfn[MAXPATHLEN];
		if(realpath(dst,rfn) && findfilesubdir1(rfn,MAXPATHLEN,subdir,ext)){
			strncpy(dst,rfn,FILELEN);
			return 1;
		}
	}
#endif
	return 0;
}

/***************************** getfiles ***************************************/

void fthumbinit(struct imgfile *ifl){
	strncpy(ifl->tfn,ifl->fn,FILELEN);
	if(!findfilesubdir(ifl->tfn,"thumb",NULL)){
		ifl->tfn[0]='\0';
		debug(DBG_DBG,"thumbinit no thumb found for '%s'",ifl->fn);
	}else
		debug(DBG_DBG,"thumbinit thumb used: '%s'",ifl->tfn);
}

void faddfile(char *fn){
	struct img *img=imgadd();
	if(!strncmp(fn,"file://",7)) fn+=7;
	strncpy(img->file->fn,fn,FILELEN);
	fthumbinit(img->file);
	imgpanoload(img->pano,fn);
}

void faddflst(char *flst){
	FILE *fd=fopen(flst,"r");
	char buf[FILELEN];
	if(!fd){ error(ERR_CONT,"ld read flst failed \"%s\"",flst); return; }
	while(!feof(fd)){
		int len;
		if(!fgets(buf,FILELEN,fd)) continue;
		len=strlen(buf);
		while(buf[len-1]=='\n' || buf[len-1]=='\r') buf[--len]='\0';
		faddfile(buf);
	}
	fclose(fd);
}

void fgetfiles(int argc,char **argv){
	char *defimgfn = finddatafile("defimg.png");
	if(!defimgfn) defimgfn="";
	defimg=imginit();
	strncpy(defimg->file->fn,defimgfn,FILELEN);
	for(;argc;argc--,argv++){
		if(!strcmp(".flst",argv[0]+strlen(argv[0])-5)) faddflst(argv[0]);
		else faddfile(argv[0]);
	}
	if(cfggetint("ld.random")) imgrandom();
	actadd(ACT_LOADMARKS,NULL);
}
