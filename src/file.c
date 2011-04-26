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

/***************************** imgfile ******************************************/

struct imgfile {
	char fn[FILELEN];
	char tfn[FILELEN];
	char dir[FILELEN];
	struct txtimg txt;
	char delfn[FILELEN];
};

struct imgfile *imgfileinit(){ return calloc(1,sizeof(struct imgfile)); }
void imgfilefree(struct imgfile *ifl){ free(ifl); }

/* thread: dpl, load */
char *imgfilefn(struct imgfile *ifl){ return ifl->fn; }
char *imgfiledelfn(struct imgfile *ifl){ return ifl->delfn; }
const char *imgfiledir(struct imgfile *ifl){ return ifl->dir[0] ? ifl->dir : NULL; }
struct txtimg *imgfiletxt(struct imgfile *ifl){ return ifl->txt.txt[0] ? &ifl->txt : NULL; }

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
			snprintf(dst,len,fn);
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
			snprintf(dst,FILELEN,rfn);
			return 1;
		}
	}
#endif
	return 0;
}

/***************************** getfiles ***************************************/

char fthumbchecktime(struct imgfile *ifl,long ft){
	if(!ifl->tfn[0]) return 0;
	if(ft<0) ft=filetime(ifl->fn);
	if(ft<=filetime(ifl->tfn)) return 1;
	ifl->tfn[0]='\0';
	debug(DBG_DBG,"thumbinit thumb too old for '%s'",ifl->fn);
	return 0;
}

void fthumbinit(struct imgfile *ifl){
	snprintf(ifl->tfn,FILELEN,ifl->fn);
	if(!findfilesubdir(ifl->tfn,"thumb",NULL)){
		ifl->tfn[0]='\0';
		debug(DBG_DBG,"thumbinit no thumb found for '%s'",ifl->fn);
	}else if(fthumbchecktime(ifl,-1))
		debug(DBG_DBG,"thumbinit thumb used: '%s'",ifl->tfn);
}

int faddfile(struct imglist *il,const char *fn,struct imglist *src){
	struct img *img;
	size_t len;
	char *prg;
	if(!strncmp(fn,"file://",7)) fn+=7;
	len=strlen(fn);
	if((prg=strchr(fn,';'))){
		len=(size_t)(prg-fn);
		prg++;
		if(len==3 && !strncmp(fn,"frm",3)) ilprgfrm(il,prg);
	}
	if(len>=FILELEN) return 0;
	if(isdir(fn)){
		int i=(int)len-2, l;
		img=imgadd(il,prg);
		memcpy(img->file->fn,fn,len); img->file->fn[len]='\0';
		while(i>=0 && img->file->fn[i]!='/' && img->file->fn[i]!='\\') i--;
		for(l=0,i++;l<FILELEN-1 && fn[i];l++,i++){
			char c=img->file->fn[i];
			if((c=='.' || c=='/' || c=='\\') && l>=2) break;
			if(c=='_') c=' ';
			img->file->dir[l]=c;
		}
		img->file->dir[l]='\0';
		utf8check(img->file->dir);
		debug(DBG_DBG,"directory found '%s': '%s'",img->file->dir,img->file->fn);
	}else if(len>=5 && !strncmp(fn,"txt_",4)){
		char *pos,*end;
		size_t ltxt;
		int i;
		float val;
		img=imgadd(il,prg);
		pos=strchr(fn+=4,'_');
		ltxt = MIN(pos ? (size_t)pos-(size_t)fn : len, FILELEN-1);
		memcpy(img->file->txt.txt,fn,ltxt);
		img->file->txt.txt[ltxt]='\0';
		utf8check(img->file->txt.txt);
		for(i=0;i<4;i++){
			while(*pos=='_') pos++;
			val=(float)strtod(pos,&end);
			if(pos==end) break;
			img->file->txt.col[i]=val;
			pos=end;
		}
		for(;i<4;i++) img->file->txt.col[i]=1.f;
	}else{
		char ok=0;
		if(fileext(fn,len,".png")) ok=1;
		if(fileext(fn,len,".jpg")) ok=1;
		if(fileext(fn,len,".jpeg")) ok=1;
		if(!ok) return 0;
		if(!src || !ilmoveimg(il,src,fn,len)){
			img=imgadd(il,prg);
			memcpy(img->file->fn,fn,len); img->file->fn[len]='\0';
			fthumbinit(img->file);
			imgpanoload(img->pano,fn);
			imgposmark(img,MPC_YES);
		}
	}
	return 1;
}

int faddflst(struct imglist *il,const char *flst,const char *pfx,struct imglist *src){
	FILE *fd;
	char buf[LDFILELEN];
	int count=0;
	size_t lpfx=strlen(pfx);
	char prg;
	if((prg=fileext(flst,0,".effprg"))){
		char cmd[FILELEN*3];
		snprintf(cmd,FILELEN*3,"perl \"%s\" \"%s\"",finddatafile("effprg.pl"),flst);
		if(!(fd=popen(cmd,"r"))){ error(ERR_CONT,"ld read effprg failed \"%s\"",cmd); return 0; }
	}else
		if(!(fd=fopen(flst,"r"))){ error(ERR_CONT,"ld read flst failed \"%s\"",flst); return 0; }
	while(!feof(fd)){
		size_t len;
		if(!fgets(buf,LDFILELEN,fd)) continue;
		len=strlen(buf);
		while(len && (buf[len-1]=='\n' || buf[len-1]=='\r')) buf[--len]='\0';
		if(!len) continue;
#ifdef __WIN32__
		if(buf[1]!=':' && buf[2]!='\\')
#else
		if(buf[0]!='/')
#endif
			if(strncmp(buf,"frm;",4) && strncmp(buf,"txt_",4))
		{
			memmove(buf+lpfx,buf,MIN(len+1,LDFILELEN-lpfx-1));
			memcpy(buf,pfx,lpfx);
			buf[LDFILELEN-1]='\0';
		}
		count+=faddfile(il,buf,src);
	}
	if(prg) pclose(fd); else fclose(fd);
	return count;
}

void finitimg(struct img **img,const char *basefn){
	const char *fn = finddatafile(basefn);
	if(!fn) fn="";
	*img=imginit();
	snprintf((*img)->file->fn,FILELEN,fn);
}

/* thread: dpl */
void floadfinalize(struct imglist *il,char sort){
	if(cfggetint(sort?"ld.datesortdir":"ld.datesort")) imgsort(il,1);
	else if(cfggetint("ld.random")) imgrandom(il);
	else if(sort) imgsort(il,0);
}

void fgetfiles(int argc,char **argv){
	struct imglist *il;
	int i;
	finitimg(&defimg,"defimg.png");
	finitimg(&dirimg,"dirimg.png");
	if(argc==1 && isdir(argv[0]) && (il=floaddir(argv[0],""))){
		ilswitch(il);
		return;
	}
	il=ilnew("[BASE]","");
	for(i=0;i<argc;i++){
		if(fileext(argv[i],0,".flst")) faddflst(il,argv[i],"",NULL);
		else if(argc==1 && fileext(argv[i],0,".effprg")) faddflst(il,argv[i],"",NULL);
		else faddfile(il,argv[i],NULL);
	}
	floadfinalize(il,0);
	ilswitch(il);
}

/* thread: dpl */
struct imglist *floaddir(const char *fn,const char *dir){
#if HAVE_OPENDIR
	DIR *dd;
	FILE *fd;
	struct dirent *de;
	char buf[FILELEN];
	size_t ld;
	struct imglist *il=NULL;
	struct imglist *src=NULL;
	int count=0;
	if(ilfind(fn,&src)) return src;
	if(!(dd=opendir(fn)) && !(fd=fopen(fn,"r"))){
		error(ERR_CONT,"opendir failed (%s)",fn);
		goto end;
	}
	if(!dd) fclose(fd);
	il=ilnew(fn,dir);
	if(!dd) count=faddflst(il,fn,"",src); else{
		ld=strlen(fn);
		memcpy(buf,fn,ld);
		if(ld && buf[ld-1]!='/' && buf[ld-1]!='\\' && ld<FILELEN) buf[ld++]='/';
		while((de=readdir(dd))){
			size_t l=0;
			while(l<NAME_MAX && de->d_name[l]) l++;
			if(l>=1 && de->d_name[0]=='.') continue;
			if(ld+l>=LDFILELEN) continue;
			memcpy(buf+ld,de->d_name,l);
			buf[ld+l]='\0';
			count+=faddfile(il,buf,src);
		}
	}
	if(dd) closedir(dd);
	if(!count){ ildestroy(il); il=NULL; }
	else floadfinalize(il,1);
end:
	if(src) ilunused(src);
	return il;
#endif
}

/* thread: dpl */
char fimgswitchmod(struct img *img){
	struct imgfile *ifl=img->file;
	char  fn[FILELEN];
	char *pos;
	size_t l;
	if(ifl->dir[0] || ifl->txt.txt[0]) return 0;
	if(!(pos=strrchr(ifl->fn,'.'))) return 0;
	l=(size_t)(pos-ifl->fn);
	memcpy(fn,ifl->fn,l);
	snprintf(fn+l,FILELEN-l,"_mod%s",pos);
	if(!isfile(fn)) return 0;
	snprintf(ifl->delfn,FILELEN,ifl->fn);
	snprintf(ifl->fn,FILELEN,fn);
	fthumbinit(ifl);
	imgpanoload(img->pano,fn);
	imgposmark(img,MPC_RESET);
	imgldfiletime(img->ld,FT_RESET);
	return 1;
}
