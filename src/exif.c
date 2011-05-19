#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if HAVE_EXIF
	#include <exif-data.h>
#endif
#if HAVE_MKTIME
	#include <time.h>
#endif
#include "exif.h"
#include "main.h"
#include "cfg.h"
#include "help.h"
#include "dpl.h"

#if HAVE_EXIF
struct exifinfo {
	const char *name;
	ExifTag tag[4];
} exifinfo[]={
	{__("Date"), {EXIF_TAG_DATE_TIME_ORIGINAL,0}},
	{__("Model"), {EXIF_TAG_MODEL,0}},
	{__("Resolution"), {EXIF_TAG_PIXEL_X_DIMENSION,EXIF_TAG_PIXEL_Y_DIMENSION,0}},
	{__("ISO speed rating"), {EXIF_TAG_ISO_SPEED_RATINGS,0}},
	{__("Focal length"), {EXIF_TAG_FOCAL_LENGTH,0}},
	{__("Exposure time"), {EXIF_TAG_EXPOSURE_TIME,0}},
	{__("Fnumber"), {EXIF_TAG_FNUMBER,0}},
	{__("Exposure bias value"), {EXIF_TAG_EXPOSURE_BIAS_VALUE,0}},
	{__("Flash"), {EXIF_TAG_FLASH,0}},
	{__("Exposure program"), {EXIF_TAG_EXPOSURE_PROGRAM,0}},
	{__("Exposure mode"), {EXIF_TAG_EXPOSURE_MODE,0}},
	{__("White blanace"), {EXIF_TAG_WHITE_BALANCE,0}},
	{NULL, {0}},
};
#endif

struct imgexif {
	char load;
	enum rot rot;
	int64_t date;
	char *info;
	struct imglist *sortil;
};

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

/* thread: dpl */
void imgexifsetsortil(struct imgexif *exif,struct imglist *sortil){ if(!exif->load) exif->sortil=sortil; }

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
enum rot imgexifgetrot(ExifData *exdat){
	ExifEntry *exet=exif_data_get_entry(exdat,EXIF_TAG_ORIENTATION);
	if(!exet) return ROT_0;
	if(exet->format!=EXIF_FORMAT_SHORT)
	{ error(ERR_CONT,"wrong orientation format (%i)",exet->format); return ROT_0; }
	if(exet->components<1)
	{ error(ERR_CONT,"orientation tag with too less components (%lu)",exet->components); return ROT_0; }
	switch(*(short*)exet->data){
	case ER_R90:  return ROT_90;
	case ER_R180: return ROT_180;
	case ER_R270: return ROT_270;
	}
	return ROT_0;
}

/* thread: load */
int64_t imgexifgetdate(ExifData *exdat){
#if HAVE_MKTIME
	ExifEntry *exet=exif_data_get_entry(exdat,EXIF_TAG_DATE_TIME_ORIGINAL);
	char *buf,*pos,c=0;
	struct tm tm;
	int64_t ret;
	if(!exet) exet=exif_data_get_entry(exdat,EXIF_TAG_DATE_TIME);
	if(!exet) exet=exif_data_get_entry(exdat,EXIF_TAG_DATE_TIME_DIGITIZED);
	if(!exet) return 0;
	if(exet->format!=EXIF_FORMAT_ASCII) return 0;
	pos=buf=malloc(exet->components+1);
	memcpy(buf,exet->data,exet->components);
	buf[exet->components]='\0';
	memset(&tm,0,sizeof(struct tm));
	tm.tm_isdst=-1;
	while(*pos && c<6){
		long int val;
		char *nxt=NULL;
		while(*pos==':' || *pos=='.' || *pos==' ') pos++;
		val=strtol(pos,&nxt,10);
		if(!nxt || nxt==pos){ error(ERR_CONT,"date string parse error (%s)",buf); return 0; }
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
	free(buf);
	ret=mktime(&tm);
	if(ret<0) ret=0;
	return ret;
#else
	return 0;
#endif
}

/* thread: load */
#define IILEN	256
#define IIINC	1024
char *imgexifgetinfo(ExifData *exdat){
	char *imginfo=NULL;
	unsigned int iilen=0;
	unsigned int iipos=0;
	int l,i;
	for(l=0;exifinfo[l].name;l++){
		unsigned int end=iipos+IILEN;
		if(end+1>iilen) imginfo=realloc(imginfo,(iilen+=IIINC)*sizeof(char));
		snprintf(imginfo+iipos,end-iipos,exifinfo[l].name);
		utf8check(imginfo+iipos);
		iipos+=(unsigned int)strlen(imginfo+iipos)+1;
		for(i=0;i<5 && exifinfo[l].tag[i];i++){
			ExifEntry *exet=exif_data_get_entry(exdat,exifinfo[l].tag[i]);
			if(!exet) continue;
			if(i && iipos<end) imginfo[iipos++]=' '; 
			exif_entry_get_value(exet,imginfo+iipos,end-iipos);
			utf8check(imginfo+iipos);
			iipos+=(unsigned int)strlen(imginfo+iipos);
		}
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

/* thread: load */
char imgexifload(struct imgexif *exif,char *fn){
#if HAVE_EXIF
	ExifData *exdat;
	if(exif->load) return 0;
	if(exif->info) free(exif->info);
	exif->load=1;
	debug(DBG_DBG,"exif loading img %s",fn);
	if(!(exdat=exif_data_new_from_file(fn))) return 1;
	exif->rot=imgexifgetrot(exdat);
	exif->date=imgexifgetdate(exdat);
	exif->info=imgexifgetinfo(exdat);
	exif_data_free(exdat);
	if(exif->sortil){
		dplsetresortil(exif->sortil);
		exif->sortil=NULL;
	}
	return 1;
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
