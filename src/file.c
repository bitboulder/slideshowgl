#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#if HAVE_REALPATH
	#include <sys/param.h>
	#include <unistd.h>
#endif
#if HAVE_OPENDIR
	#include <sys/types.h>
	#include <dirent.h>
	#ifndef NAME_MAX
		#define NAME_MAX 255
	#endif
#endif
#include "file.h"
#include "main.h"
#include "img.h"
#include "cfg.h"
#include "act.h"
#include "pano.h"
#include "help.h"
#include "dpl.h"
#include "eff.h"
#include "mark.h"

/***************************** imgfile ******************************************/

struct imgfile {
	char fn[FILELEN];
	char tfn[FILELEN];
	const char *dir;
};

struct imgfile *imgfileinit(){ return calloc(1,sizeof(struct imgfile)); }
void imgfilefree(struct imgfile *ifl){ free(ifl); }

/* thread: dpl, load */
char *imgfilefn(struct imgfile *ifl){ return ifl->fn; }
const char *imgfiledir(struct imgfile *ifl){ return ifl->dir; }

/* thread: load */
char imgfiletfn(struct imgfile *ifl,char **tfn){
	if(!ifl->tfn[0]) return 0;
	*tfn=ifl->tfn;
	return 1;
}

/***************************** find *******************************************/

char findfilesubdir1(char *dst,unsigned int len,const char *subdir,const char *ext){
	size_t i;
	FILE *fd;
	char fn[FILELEN];
	char *extpos = ext ? strrchr(dst,'.') : NULL;
	if(extpos>dst+4 && !strncmp(extpos-4,"_cut",4)) extpos-=4;
	if(extpos>dst+6 && !strncmp(extpos-6,"_small",6)) extpos-=6;
	if(extpos) extpos[0]='\0';
	for(i=strlen(dst);i--;) if(dst[i]=='/' || dst[i]=='\\'){
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

char findfilesubdir(char *dst,const char *subdir,const char *ext){
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

int faddfile(struct imglist *il,const char *fn){
	struct img *img;
	if(!strncmp(fn,"file://",7)) fn+=7;
	if(strlen(fn)>=FILELEN) return 0;
	if(isdir(fn)){
		int i=(int)strlen(fn)-2;
		img=imgadd(il);
		strncpy(img->file->fn,fn,FILELEN);
		while(i>=0 && img->file->fn[i]!='/' && img->file->fn[i]!='\\') i--;
		img->file->dir=img->file->fn+i+1;
		debug(DBG_DBG,"directory found: '%s'",img->file->fn);
	}else{
		size_t l=strlen(fn);
		char ok=0;
		if(l>=5 && !strncmp(fn+l-4,".png",4)) ok=1;
		if(l>=5 && !strncmp(fn+l-4,".jpg",4)) ok=1;
		if(l>=6 && !strncmp(fn+l-5,".jpeg",5)) ok=1;
		if(!ok) return 0;
		img=imgadd(il);
		strncpy(img->file->fn,fn,FILELEN);
		fthumbinit(img->file);
		imgpanoload(img->pano,fn);
		markimgload(img);
	}
	return 1;
}

int faddflst(struct imglist *il,char *flst,const char *pfx){
	FILE *fd=fopen(flst,"r");
	char buf[FILELEN];
	int count=0;
	size_t lpfx=strlen(pfx);
	if(!fd){ error(ERR_CONT,"ld read flst failed \"%s\"",flst); return 0; }
	while(!feof(fd)){
		size_t len;
		if(!fgets(buf,FILELEN,fd)) continue;
		len=strlen(buf);
		while(len && (buf[len-1]=='\n' || buf[len-1]=='\r')) buf[--len]='\0';
		if(!len) continue;
#ifdef __WIN32__
		if(buf[1]!=':' && buf[2]!='\\')
#else
		if(buf[0]!='/')
#endif
		{
			memmove(buf+lpfx,buf,MIN(len+1,FILELEN-lpfx-1));
			memcpy(buf,pfx,lpfx);
			buf[FILELEN-1]='\0';
		}
		count+=faddfile(il,buf);
	}
	fclose(fd);
	return count;
}

void finitimg(struct img **img,const char *basefn){
	const char *fn = finddatafile(basefn);
	if(!fn) fn="";
	*img=imginit();
	strncpy((*img)->file->fn,fn,FILELEN);
}

void floadfinalize(struct imglist *il,char sort){
	if(cfggetint(sort?"ld.datesortdir":"ld.datesort")) imgsort(il,1);
	else if(cfggetint("ld.random")) imgrandom(il);
	else if(sort) imgsort(il,0);
}

void fgetfiles(int argc,char **argv){
	struct imglist *il=ilnew("[BASE]");
	finitimg(&defimg,"defimg.png");
	finitimg(&dirimg,"dirimg.png");
	for(;argc;argc--,argv++){
		if(!strcmp(".flst",argv[0]+strlen(argv[0])-5)) faddflst(il,argv[0],"");
		else faddfile(il,argv[0]);
	}
	floadfinalize(il,0);
	ilswitch(il);
}

/* thread: dpl */
int floaddir(struct imgfile *ifl){
#if HAVE_OPENDIR
	DIR *dd;
	FILE *fd;
	struct dirent *de;
	char buf[FILELEN];
	size_t ld;
	struct imglist *il;
	int count=0;
	if((il=ilfind(ifl->fn))) return ilswitch(il);
	if(!(dd=opendir(ifl->fn)) && !(fd=fopen(ifl->fn,"r"))){
		error(ERR_CONT,"opendir failed (%s)",ifl->fn);
		return IMGI_END;
	}
	if(!dd) fclose(fd);
	il=ilnew(ifl->fn);
	ld=strlen(ifl->fn);
	memcpy(buf,ifl->fn,ld);
	if(dd){
		if(buf[ld-1]!='/' && buf[ld-1]!='\\' && ld<FILELEN) buf[ld++]='/';
	}else{
		while(buf[ld-1]!='/' && buf[ld-1]!='\\') ld--;
		buf[ld]='\0';
	}
	if(!dd) count=faddflst(il,ifl->fn,buf); else while((de=readdir(dd))){
		size_t l=0;
		while(l<NAME_MAX && de->d_name[l]) l++;
		if(l>=1 && de->d_name[0]=='.') continue;
		if(ld+l>=FILELEN) continue;
		memcpy(buf+ld,de->d_name,l);
		buf[ld+l]='\0';
		count+=faddfile(il,buf);
	}
	if(dd) closedir(dd);
	if(!count){ ildestroy(il); return IMGI_END; }
	floadfinalize(il,1);
	return ilswitch(il);
#endif
}
