#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if HAVE_EXIV2
	#include "exiv2.h"
#elif HAVE_EXIF
	#include <exif-data.h>
#endif
#if HAVE_MKTIME
	#include <time.h>
#endif
#include "exif_int.h"
#include "main.h"
#include "cfg.h"
#include "help.h"
#include "dpl.h"
#include "eff.h"
#include "act.h"
#include "exich.h"

#if HAVE_EXIV2
	typedef struct exiv2data exifdata;
	typedef int ExifTag;
	#define E(X)	0
#else
	typedef ExifData exifdata;
	#define E(X)	X
#endif

struct exifinfo {
	enum exinfo { EX_XX, EX_DT, EX_FL, EX_ET, EX_FN, EX_IO, EX_EV, EX_FA, EX_FV, EX_EP, EX_PS, EX_TP } info;
	const char *name;
	const char *tag2[8];
	ExifTag tag1[4];
} exifinfo[] = {
	{EX_DT, __("Date"),                {"Exif.Photo.DateTimeOriginal","Exif.Image.DateTime","Exif.Photo.DateTimeDigitized",NULL},
								{E(EXIF_TAG_DATE_TIME_ORIGINAL),0}},

	{EX_XX, __("Model"),               {"Exif.Image.Model",NULL},            {E(EXIF_TAG_MODEL),0}},
	{EX_XX, __("Resolution"),          {"join:x","Exif.Photo.PixelXDimension","Exif.Photo.PixelYDimension",NULL},
								{E(EXIF_TAG_PIXEL_X_DIMENSION),E(EXIF_TAG_PIXEL_Y_DIMENSION),0}},

	{EX_XX, __("Lens"),                {"Easy.LensName",NULL},               {0}},
	{EX_FL, __("Focal length"),        {"Easy.FocalLength",NULL},            {E(EXIF_TAG_FOCAL_LENGTH),0}},

	{EX_ET, __("Exposure time"),       {"Easy.ExposureTime",NULL},           {E(EXIF_TAG_EXPOSURE_TIME),0}},
	{EX_FN, __("Fnumber"),             {"Easy.FNumber",NULL},                {E(EXIF_TAG_FNUMBER),0}},
	{EX_IO, __("ISO speed rating"),    {"Easy.IsoSpeed",NULL},               {E(EXIF_TAG_ISO_SPEED_RATINGS),0}},
	{EX_EV, __("Exposure bias value"), {"Exif.Photo.ExposureBiasValue",NULL},{E(EXIF_TAG_EXPOSURE_BIAS_VALUE),0}},
	{EX_XX, __("Metering mode"),       {"Easy.MeteringMode",NULL},           {0}},
	{EX_XX, __("Measured EV"),         {"Exif.CanonSi.MeasuredEV2",NULL},    {0}},

	{EX_FA, __("Flash"),               {"Exif.Photo.Flash",NULL},            {E(EXIF_TAG_FLASH),0}},
	{EX_FV, __("Flash bias value"),    {"Exif.CanonSi.FlashBias",NULL},      {0}},

	{EX_EP, __("Exposure program"),    {"Exif.CanonCs.ExposureProgram","Exif.Photo.ExposureProgram",NULL},{E(EXIF_TAG_EXPOSURE_PROGRAM),0}},
	{EX_XX, __("Exposure mode"),       {"Exif.Photo.ExposureMode",NULL},     {E(EXIF_TAG_EXPOSURE_MODE),0}},
	{EX_XX, __("White blanace"),       {"Exif.CanonSi.WhiteBalance","Exif.Photo.WhiteBalance",NULL},{E(EXIF_TAG_WHITE_BALANCE),0}},
	{EX_PS, __("Picture style"),       {"Exif.CanonPr.PictureStyle",NULL},   {0}},
	{EX_TP, __("Tone priority"),       {"get:25","onoff:","Exif.Canon.CustomFunctions",NULL},{0}},

	{EX_XX, __("Focus mode"),          {"Exif.CanonCs.FocusMode",NULL},      {0}},
	{EX_XX, __("Drive mode"),          {"Exif.CanonCs.DriveMode",NULL},      {0}},
	{EX_XX, NULL, {NULL}},
};

#undef E



/* thread: all */
enum rot imgexifrot(struct imgexif *exif){ return exif->rot; }
int64_t imgexifdate(struct imgexif *exif){ return exif->date; }
char *imgexifinfo(struct imgexif *exif){ return exif->info; }

/* thread: gl, dpl */
float imgexifrotf(struct imgexif *exif){
	switch(exif->rot){
	case ROT_0:   return 0.f;
	case ROT_90:  return 90.f;
	case ROT_180: return 180.f;
	case ROT_270: return 270.f;
	}
	return 0.f;
}

/* thread: img */
struct imgexif *imgexifinit(){
	return calloc(1,sizeof(struct imgexif));
}

/* thread: img */
void imgexiffree(struct imgexif *exif){
	if(exif->info) free(exif->info);
	free(exif);
}

#if HAVE_EXIF
enum exifrot {
	//action		0'Row	0'Col
	ER_UDEF =0,	//	nothing
	ER_NONE =1,	//	top		left
	ER_FLPH =2,	//	top		right
	ER_R180 =3,	//	bottom	right
	ER_FLPV =4,	//	bottom	left
	ER_TMAIN=5,	//	left	top
	ER_R90  =6,	//	right	top
	ER_TSLAV=7,	//	right	bottom
	ER_R270 =8,	//	left	bottom
};

/* thread: load */
enum rot imgexifgetrot(exifdata *exdat){
	long rot;
#if HAVE_EXIV2
	rot=exiv2getint(exdat,"Exif.Image.Orientation");
#elif HAVE_EXIF
	ExifEntry *exet=exif_data_get_entry(exdat,EXIF_TAG_ORIENTATION);
	if(!exet) return ROT_0;
	if(exet->format!=EXIF_FORMAT_SHORT)
	{ error(ERR_CONT,"wrong orientation format (%i)",exet->format); return ROT_0; }
	if(exet->components<1)
	{ error(ERR_CONT,"orientation tag with too less components (%lu)",exet->components); return ROT_0; }
	rot=*(short*)exet->data;
#endif
	switch(rot){
	case ER_R90:  return ROT_90;
	case ER_R180: return ROT_180;
	case ER_R270: return ROT_270;
	}
	return ROT_0;
}

/* thread: load */
int64_t imgexifgetdate(exifdata *exdat){
#if HAVE_MKTIME
	char *pos,c=0;
	struct tm tm;
	int64_t ret;
#if HAVE_EXIV2
	char buf[64];
	if(!exiv2getstr(exdat,exifinfo[0].tag2,buf,64)) return 0;
#elif HAVE_EXIF
	char *buf;
	ExifEntry *exet=exif_data_get_entry(exdat,EXIF_TAG_DATE_TIME_ORIGINAL);
	if(!exet) exet=exif_data_get_entry(exdat,EXIF_TAG_DATE_TIME);
	if(!exet) exet=exif_data_get_entry(exdat,EXIF_TAG_DATE_TIME_DIGITIZED);
	if(!exet) return 0;
	if(exet->format!=EXIF_FORMAT_ASCII) return 0;
	buf=malloc(exet->components+1);
	memcpy(buf,exet->data,exet->components);
	buf[exet->components]='\0';
#endif
	memset(&tm,0,sizeof(struct tm));
	tm.tm_isdst=-1;
	pos=buf;
	while(*pos && c<6){
		long int val;
		char *nxt=NULL;
		while(*pos==':' || *pos=='.' || *pos==' ') pos++;
		val=strtol(pos,&nxt,10);
		if(!nxt || nxt==pos){ error(ERR_CONT,"date string parse error (in \"%s\" at \"%s\")",buf,pos); return 0; }
		pos=nxt;
		switch(c++){
		case 0: tm.tm_year=(int)val-1900; break;
		case 1: tm.tm_mon=(int)val-1; break;
		case 2: tm.tm_mday=(int)val; break;
		case 3: tm.tm_hour=(int)val; break;
		case 4: tm.tm_min=(int)val; break;
		case 5: tm.tm_sec=(int)val; break;
		}
	}
	if(c<2){ error(ERR_CONT,"no enought digits for date (%s)",buf); return 0; }
#if ! HAVE_EXIV2 && HAVE_EXIF
	free(buf);
#endif
	ret=mktime(&tm);
	if(ret<0) ret=0;
	return ret;
#else
	return 0;
#endif
}

double frac2f(char *txt){
	double n=atof(txt);
	char *dp=strchr(txt,'/');
	double d=dp?atof(dp+1):1.f;
	return n/d;
}

/* thread: load */
void imgexifselupd(char *sel,enum exinfo info,char *txt){
#define selp(p)				(sel+16*(p))
#define SEL(p,txt)			SELl(p,100,txt)
#define SELf(p,fmt,txt)		SELlf(p,100,fmt,txt)
#define SELl(p,l,txt)		SELlf(p,l,"%s",txt)
#define SELlf(p,l,fmt,txt)	snprintf(selp(p),MIN(16,l+1),fmt,txt)
#define SELlp(p,l,txt,prf)	{ SELl(p,l,txt); SELap(p,prf); }
#define SELap(p,prf)		SELfap(p,"%s",prf)
#define SELfap(p,fmt,prf)	{ size_t w=strlen(selp(p)); snprintf(selp(p)+w,16-w,fmt,prf); }
	size_t l;
	switch(info){
	case EX_XX: break;
	case EX_DT:
		if((l=strcspn(txt," "))){
			SELl(0,l,txt);
			SEL(1,txt+l+1);
		}else SEL(0,txt);
	break;
	case EX_FL: if((l=strspn(txt,"0123456789"))) SELlp(3,l,txt,"mm") else SEL(3,""); break;
	case EX_ET: if((l=strspn(txt,"0123456789/"))) SELl(5,l,txt); else SEL(5,""); break;
	case EX_FN: SELf(4,"%.1f",atof(txt+(txt[0]=='F'))); break;
	case EX_IO: SEL(7,txt); break;
	case EX_EV: SELf(6,"%+.1f",frac2f(txt)); break;
	case EX_FA: if(!strstr(txt,"nicht")) SEL(9,"B"); else SEL(9,""); break;
	case EX_FV: if(selp(9)[0]=='B') SELfap(9,"%+.1f",frac2f(txt)); break;
	case EX_EP:
		l=strcspn(txt,"(");
		if(txt[l]){
			txt+=l+1;
			l=strcspn(txt,")");
			SELl(2,l,txt);
		}
		else if(!strcmp(txt,"A-DEP")) SEL(2,"Adep");
		else if(!strcmp(txt,"Blendenpriorität")) SEL(2,"Av");
		else if(!strcmp(txt,"Verschlußpriorität")) SEL(2,"Tv");
		else if(!strcmp(txt,"Manuell")) SEL(2,"M");
		else if(!strcmp(txt,"Undefiniert")) SEL(2,"Undef");
		else SELl(2,4,txt);
	break;
	case EX_PS: SELl(8,1,txt); break;
	case EX_TP: if(!strncmp(txt,"on",2)) SELap(7,"+"); break;
	}
}

/* thread: load */
#define IILEN	256
#define IIINC	1024
char *imgexifgetinfo(exifdata *exdat){
	char *imginfo=calloc(ISLEN,1);
	unsigned int iilen=ISLEN;
	unsigned int iipos=ISLEN;
	int l;
#if ! HAVE_EXIV2 && HAVE_EXIF
	int i;
#endif
	for(l=0;exifinfo[l].name;l++){
		unsigned int end=iipos+IILEN;
		if(end+1>iilen) imginfo=realloc(imginfo,(iilen+=IIINC)*sizeof(char));
		snprintf(imginfo+iipos,end-iipos,exifinfo[l].name);
		utf8check(imginfo+iipos);
		iipos+=(unsigned int)strlen(imginfo+iipos)+1;
#if HAVE_EXIV2
		if(exiv2getstr(exdat,exifinfo[l].tag2,imginfo+iipos,(int)(end-iipos))){
			utf8check(imginfo+iipos);
			imgexifselupd(imginfo,exifinfo[l].info,imginfo+iipos);
			iipos+=(unsigned int)strlen(imginfo+iipos);
		}
#elif HAVE_EXIF
		for(i=0;i<5 && exifinfo[l].tag1[i];i++){
			ExifEntry *exet=exif_data_get_entry(exdat,exifinfo[l].tag1[i]);
			if(!exet) continue;
			if(i && iipos<end) imginfo[iipos++]=' '; 
			exif_entry_get_value(exet,imginfo+iipos,end-iipos);
			utf8check(imginfo+iipos);
			imgexifselupd(imginfo,exifinfo[l].info,imginfo+iipos);
			iipos+=(unsigned int)strlen(imginfo+iipos);
		}
#endif
		if(iipos && imginfo[iipos-1]=='\0'){
			snprintf(imginfo+iipos,end-iipos,_("(unknown)"));
			utf8check(imginfo+iipos);
			iipos+=(unsigned int)strlen(imginfo+iipos);
		}
		iipos++;
	}
	if(imginfo) imginfo[iipos]='\0';
	return imginfo;
}
#endif

/* thread: load,dpl */
char imgexifload(struct imgexif *exif,const char *fn){
#if HAVE_EXIV2 || HAVE_EXIF
	enum rot r;
	int64_t d;
	exifdata *exdat;
	char ret=0x1;
	enum filetype ft=filetype(fn);
	if(exif->load) return 0;
	if(!(ft&FT_FILE) || (ft&FT_DIREX)) return 1;
	if(exif->info){ free(exif->info); exif->info=NULL; }
	exif->load=1;
	debug(DBG_DBG,"exif loading img %s",fn);
#if HAVE_EXIV2
	if(!(exdat=exiv2init(fn))) return 1;
#elif HAVE_EXIF
	if(!(exdat=exif_data_new_from_file(fn))) return 1;
#endif
	r=imgexifgetrot(exdat);
	if(r!=exif->rot){ ret|=0x2; exif->rot=r; }
	d=imgexifgetdate(exdat);
	if(d!=exif->date){ ret|=0x4; exif->date=d; }
	exif->info=imgexifgetinfo(exdat);
#if HAVE_EXIV2
	exiv2free(exdat);
#elif HAVE_EXIF
	exif_data_free(exdat);
#endif
	exichadd(exif,fn);
	return ret;
#else
	return 0;
#endif
}

/* thread: load */
void imgexifclear(struct imgexif *exif){ exif->load=0; }

/* thread: dpl */
void exifrotate(struct imgexif *exif,int r){
	exif->rot = ((int)exif->rot+r+4)%4;
}
