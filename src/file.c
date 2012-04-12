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
#include "map.h"

#define IMGEXT		"jpg", "jpeg", "png", "tif", "tiff"
#define MOVEXT		"mov", "avi", "mpg"
#define WAVEXT		"wav", "mp3", "ogg"
#define SLAVEEXT	MOVEXT, "thm"
const char *const imgext[]  ={ IMGEXT,   NULL };
const char *const slaveext[]={ SLAVEEXT, NULL };
const char *const movext[]  ={ MOVEXT,   NULL };
const char *const wavext[]  ={ WAVEXT,   NULL };

/***************************** imgfile ******************************************/

struct imgfile {
	char fn[FILELEN];
	char tfn[FILELEN];
	char dir;
	char imgtxt[FILELEN];
	char mov[FILELEN];
	struct txtimg txt;
	char delfn[FILELEN];
};

struct imgfile *imgfileinit(){ return calloc(1,sizeof(struct imgfile)); }
void imgfilefree(struct imgfile *ifl){ free(ifl); }

/* thread: dpl, load */
char *imgfilefn(struct imgfile *ifl){ return ifl->fn; }
char *imgfiledelfn(struct imgfile *ifl){ return ifl->delfn; }
const char *imgfiledir(struct imgfile *ifl){ return ifl->dir ? ifl->imgtxt : NULL; }
const char *imgfilemov(struct imgfile *ifl){ return ifl->mov[0] ? ifl->mov : NULL; }
const char *imgfileimgtxt(struct imgfile *ifl){ return ifl->imgtxt[0] ? ifl->imgtxt : NULL; }
struct txtimg *imgfiletxt(struct imgfile *ifl){ return ifl->txt.txt[0] ? &ifl->txt : NULL; }

/* thread: load */
char imgfiletfn(struct imgfile *ifl,const char **tfn){
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
	char extrpl=0;
	if(extpos>dst+4 && !strncmp(extpos-4,"_mod",4)) extpos-=4;
	if(extpos>dst+4 && !strncmp(extpos-4,"_cut",4)) extpos-=4;
	if(extpos>dst+6 && !strncmp(extpos-6,"_small",6)) extpos-=6;
	if(extpos){
		extrpl=extpos[0];
		extpos[0]='\0';
	}
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
	if(extrpl) extpos[0]=extrpl;
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

char finddirmatch(char *in,char *post,char *res,const char *basedir,char onlydir){
#if HAVE_OPENDIR
	DIR *dd;
	struct dirent *de;
	size_t len=strlen(in);
	size_t blen=strlen(basedir);
	if(!(dd=opendir(basedir))) return 0;
	while((de=readdir(dd))){
		char buf[FILELEN];
		size_t l=0;
		enum filetype ft;
		while(l<NAME_MAX && de->d_name[l]) l++;
		snprintf(buf,MAX(FILELEN,blen+1+l),"%s/%s",basedir,de->d_name);
		if(!((ft=filetype(buf))&FT_DIR) && onlydir) continue;
		if(len<=l && !strncmp(de->d_name,in,len) && (l==len || strncmp(post,de->d_name+len,l-len)<0)){
			snprintf(post,MAX(FILELEN,l-len),de->d_name+len);
			snprintf(res,FILELEN,"%s/%s%s",basedir,in,post);
		}
		if((ft&FT_DIR) && len>l && !strncmp(de->d_name,in,l) && (in[l]=='/' || in[l]=='\\')){
			finddirmatch(in+l+1,post,res,buf,onlydir);
			break;
		}
	}
	closedir(dd);
	return 0;
#endif
}

void frename(const char *fn,const char *dstdir){
	const char *base=strrchr(fn,'/');
	const char *ext=strrchr(fn,'.');
	char dfn[FILELEN];
	char sfn[FILELEN];
	int i;
	if(!mkdirm(dstdir,0)) return;
	if(!base) base=fn;
	snprintf(dfn,FILELEN,"%s/%s",dstdir,base);
	rename(fn,dfn);
	if(!ext) return;
	snprintf(sfn,FILELEN,fn);
	for(i=0;slaveext[i];i++){
		snprintf(sfn+(ext-fn),FILELEN-(size_t)(ext-fn),".%s",slaveext[i]);
		if(!(filetype(sfn)&FT_FILE)) continue;
		snprintf(dfn,FILELEN,"%s/%s",dstdir,sfn+(base-fn));
		rename(sfn,dfn);
	}
}

/***************************** getfiles ***************************************/

char fthumbchecktime(struct imgfile *ifl){
	long ft;
	if(!ifl->tfn[0]) return 0;
	ft=filetime(ifl->fn);
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
	}else if(fthumbchecktime(ifl))
		debug(DBG_DBG,"thumbinit thumb used: '%s'",ifl->tfn);
}

void fimgtxtinit(struct imgfile *ifl,const char *fn){
	int i=(int)strlen(fn)-2, l;
	while(i>=0 && fn[i]!='/' && fn[i]!='\\') i--;
	for(l=0,i++;l<FILELEN-1 && fn[i];l++,i++){
		char c=fn[i];
		if((c=='.' || c=='/' || c=='\\') && l>=2) break;
		if(c=='_') c=' ';
		ifl->imgtxt[l]=c;
	}
	ifl->imgtxt[l]='\0';
	utf8check(ifl->imgtxt);
}

void fmovinit(struct imgfile *ifl){
	const char *const *ext;
	size_t len=strlen(ifl->fn);
	int i;
	while(len>0 && ifl->fn[len-1]!='.') len--;
	if(!len){ ifl->mov[0]='\0'; return; }
	snprintf(ifl->mov+1,FILELEN-1,ifl->fn);
	for(i=0;i<2;i++){
		ifl->mov[0]=i?'w':'m';
		for(ext=i?wavext:movext;ext[0];ext++){
			snprintf(ifl->mov+len+1,FILELEN-len-1,ext[0]);
			if(filetype(ifl->mov+1)&FT_FILE) break;
		}
		if(ext[0]){
			if(i) fimgtxtinit(ifl,ifl->mov+1);
			return;
		}
	}
	ifl->mov[0]='\0';
}

int faddfile(struct imglist *il,const char *fn,struct imglist *src,char mapbase){
	struct img *img=NULL;
	size_t len;
	char *prg;
	enum filetype ft;
	if(!strncmp(fn,"file://",7)) fn+=7;
	len=strlen(fn);
	if((prg=strchr(fn,';'))){
		len=(size_t)(prg-fn);
		prg++;
		if(len==3 && !strncmp(fn,"frm",3)) ilprgfrm(il,prg);
	}
	if(len>=FILELEN) return 0;
	ft=filetype(fn);
	if(ft&FT_DIREX){
		img=imginit();
		memcpy(img->file->fn,fn,len); img->file->fn[len]='\0';
		fimgtxtinit(img->file,img->file->fn);
		img->file->dir=1;
		debug(DBG_DBG,"directory found '%s': '%s'",img->file->imgtxt,img->file->fn);
		if(mapbase && (ft&FT_DIR)) mapaddbasedir(img->file->fn,img->file->imgtxt);
	}else if(len>=5 && !strncmp(fn,"txt_",4)){
		char *pos,*end;
		size_t ltxt;
		int i;
		float val;
		img=imginit();
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
	}else if(ft&FT_FILE){
		char ok=0;
		const char *const *ext;
		for(ext=imgext;!ok && ext[0];ext++) if(fileext(fn,len,ext[0])) ok=1;
		if(!ok) return 0;
		if(!src || !ilmoveimg(il,src,fn,len)){
			img=imginit();
			memcpy(img->file->fn,fn,len); img->file->fn[len]='\0';
			fthumbinit(img->file);
			fmovinit(img->file);
			imgpanoload(img->pano,fn);
			imgposmark(img,MPC_YES);
		}
	}else if(!strncmp(fn,"[MAP]",6)){
		prg=NULL;
		img=imginit();
		memcpy(img->file->fn,fn,len); img->file->fn[len]='\0';
	}
	if(img) iladdimg(il,img,prg);
	return 1;
}

int faddflst(struct imglist *il,const char *flst,const char *pfx,struct imglist *src,char mapbase){
	FILE *fd;
	char buf[LDFILELEN];
	int count=0;
	size_t lpfx=strlen(pfx);
	char prg;
	if((prg=fileext(flst,0,"effprg"))){
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
		count+=faddfile(il,buf,src,mapbase);
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

struct imglist *ilbase=NULL;

void fgetfile(const char *fn,char singlefile){
	if(!ilbase) ilbase=ilnew("[BASE]","");
	if(fileext(fn,0,"flst")) faddflst(ilbase,fn,"",NULL,1);
	else if(singlefile && fileext(fn,0,"effprg")) faddflst(ilbase,fn,"",NULL,1);
	else faddfile(ilbase,fn,NULL,1);
}

void fgetfiles(int argc,char **argv){
	int i;
	enum filetype ft;
	char singlefile=!ilbase && argc==1;
	finitimg(&defimg,"defimg.png");
	finitimg(&dirimg,"dirimg.png");
	if(singlefile && ((ft=filetype(argv[0]))&FT_DIREX) && (ilbase=floaddir(argv[0],""))){
		if(ft&FT_DIR) mapaddbasedir(argv[0],"");
		goto end;
	}
	for(i=0;i<argc;i++) fgetfile(argv[i],singlefile);
end:
	cilset(ilbase,0);
	mapaddbasedir(NULL,NULL);
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
	if(!dir) dir="";
	if(ilfind(fn,&src,1)) return src;
	if(!(dd=opendir(fn)) && !(fd=fopen(fn,"r"))){
		error(ERR_CONT,"opendir failed (%s)",fn);
		goto end;
	}
	if(!dd) fclose(fd);
	il=ilnew(fn,dir);
	if(!dd) count=faddflst(il,fn,"",src,0); else{
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
			count+=faddfile(il,buf,src,0);
		}
	}
	if(dd) closedir(dd);
	if(!count){ ildestroy(il); il=NULL; }
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
	if(ifl->dir || ifl->txt.txt[0]) return 0;
	if(!(pos=strrchr(ifl->fn,'.'))) return 0;
	l=(size_t)(pos-ifl->fn);
	memcpy(fn,ifl->fn,l);
	snprintf(fn+l,FILELEN-l,"_mod%s",pos);
	if(!(filetype(fn)&FT_FILE)) return 0;
	snprintf(ifl->delfn,FILELEN,ifl->fn);
	snprintf(ifl->fn,FILELEN,fn);
	fthumbinit(ifl);
	fmovinit(ifl);
	imgpanoload(img->pano,fn);
	imgposmark(img,MPC_RESET);
	imgldfiletime(img->ld,FT_RESET);
	return 1;
}
