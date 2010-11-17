#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <exif-data.h>
#include "exif.h"

struct exifinfo {
	char *name;
	ExifTag tag[5];
} exifinfo[]={
	{"Datum", {EXIF_TAG_DATE_TIME_ORIGINAL,0}},
	{"Kamara", {EXIF_TAG_MODEL,0}},
	{"Auflösung", {EXIF_TAG_PIXEL_X_DIMENSION,EXIF_TAG_PIXEL_Y_DIMENSION,0}},
	{"Empfindlichkeit", {EXIF_TAG_ISO_SPEED_RATINGS,0}},
	{"Brennweite", {EXIF_TAG_FOCAL_LENGTH,0}},
	{"Belichtungszeit", {EXIF_TAG_EXPOSURE_TIME,0}},
	{"Blende", {EXIF_TAG_FNUMBER,0}},
	{"Belichtunskorrektur", {EXIF_TAG_EXPOSURE_BIAS_VALUE,0}},
	{"Blitz", {EXIF_TAG_FLASH,0}},
	{"Belichtungsprogramm", {EXIF_TAG_EXPOSURE_PROGRAM,0}},
	{"Belichtungsmodus", {EXIF_TAG_EXPOSURE_MODE,0}},
	{"Weißabgleich", {EXIF_TAG_WHITE_BALANCE,0}},
	{NULL, {0}},
};

struct imgexif {
	char load;
	enum rot rot;
	char *info;
};

/* thread: all */
enum rot imgexifrot(struct imgexif *exif){ return exif->rot; }
char *imgexifinfo(struct imgexif *exif){ return exif->info; }

/* thread: gl, dpl */
float imgexifrotf(struct imgexif *exif){
	switch(exif->rot){
	case ROT_0:   return 0.;
	case ROT_90:  return 90.;
	case ROT_180: return 180.;
	case ROT_270: return 270.;
	}
	return 0.;
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

/* thread: load */
enum rot imgexifgetrot(ExifData *exdat){
	ExifEntry *exet=exif_data_get_entry(exdat,EXIF_TAG_ORIENTATION);
	char buf[255];
	if(!exet) return ROT_0;
	exif_entry_get_value(exet,buf,255);
	if(!strncmp("top - left",    buf,10)) return ROT_0;
	if(!strncmp("left - bottom", buf,13)) return ROT_270;
	if(!strncmp("bottom - right",buf,14)) return ROT_180;
	if(!strncmp("right - top",   buf,11)) return ROT_90;
	return ROT_0;
}

/* thread: load */
#define IILEN	256
#define IIINC	1024
char *imgexifgetinfo(ExifData *exdat){
	char *imginfo=NULL;
	int iilen=0;
	int iipos=0;
	int l,i;
	for(l=0;exifinfo[l].name;l++){
		int end=iipos+IILEN;
		if(end+1>iilen) imginfo=realloc(imginfo,(iilen+=IIINC)*sizeof(char));
		snprintf(imginfo+iipos,end-iipos,exifinfo[l].name);
		iipos+=strlen(imginfo+iipos)+1;
		for(i=0;i<5 && exifinfo[l].tag[i];i++){
			ExifEntry *exet=exif_data_get_entry(exdat,exifinfo[l].tag[i]);
			if(!exet) continue;
			if(i && iipos<end) imginfo[iipos++]=' '; 
			exif_entry_get_value(exet,imginfo+iipos,end-iipos);
			iipos+=strlen(imginfo+iipos);
		}
		iipos+=strlen(imginfo+iipos)+1;
	}
	if(imginfo) imginfo[iipos]='\0';
	return imginfo;
}

/* thread: load */
void imgexifload(struct imgexif *exif,char *fn,char replace){
	ExifData *exdat;
	if(exif->load && !replace) return;
	if(exif->info) free(exif->info);
	exif->load=1;
	if(!(exdat=exif_data_new_from_file(fn))) return;
	exif->rot=imgexifgetrot(exdat);
	exif->info=imgexifgetinfo(exdat);
	exif_data_free(exdat);
}

/* thread: dpl */
void exifrotate(struct imgexif *exif,int r){
	exif->rot = (exif->rot+r+4)%4;
}
