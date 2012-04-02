#include "config.h"
#if HAVE_EXIV2

#include <exiv2/exiv2.hpp>
#include <sstream>
#include <string>
#include <stdio.h>

#include "exiv2.h"

typedef Exiv2::ExifData::const_iterator (*easyfnc)(const Exiv2::ExifData& ed);
struct exeasy {
	const char *key;
	easyfnc fnc;
} exeasy[] = {
	{ "Easy.Orientation", Exiv2::orientation },
	{ "Easy.IsoSpeed", Exiv2::isoSpeed },
	{ "Easy.FlashBias", Exiv2::flashBias },
	{ "Easy.ExposureMode", Exiv2::exposureMode },
	{ "Easy.SceneMode", Exiv2::sceneMode },
	{ "Easy.MacroMode", Exiv2::macroMode },
	{ "Easy.ImageQuality", Exiv2::imageQuality },
	{ "Easy.WhiteBalance", Exiv2::whiteBalance },
	{ "Easy.LensName", Exiv2::lensName },
	{ "Easy.Saturation", Exiv2::saturation },
	{ "Easy.Sharpness", Exiv2::sharpness },
	{ "Easy.Contrast", Exiv2::contrast },
	{ "Easy.SceneCaptureType", Exiv2::sceneCaptureType },
	{ "Easy.MeteringMode", Exiv2::meteringMode },
	{ "Easy.Make", Exiv2::make },
	{ "Easy.Model", Exiv2::model },
	{ "Easy.ExposureTime", Exiv2::exposureTime },
	{ "Easy.FNumber", Exiv2::fNumber },
	{ "Easy.SubjectDistance", Exiv2::subjectDistance },
	{ "Easy.SerialNumber", Exiv2::serialNumber },
	{ "Easy.FocalLength", Exiv2::focalLength },
	{ "Easy.AfPoint", Exiv2::afPoint },
	{ NULL, NULL }
};

struct exiv2data {
	char imgused;
	Exiv2::Image::AutoPtr img;
};

struct exiv2data *exiv2init(const char *fn){
	struct exiv2data *edata=(struct exiv2data *)calloc(1,sizeof(struct exiv2data));
	try {
	    edata->img = Exiv2::ImageFactory::open(fn);
		edata->imgused = 1;
		if(!edata->img.get()){
			exiv2free(edata);
			return NULL;
		}
		edata->img->readMetadata();
		Exiv2::ExifData &ed = edata->img->exifData();
		if(ed.empty()){
			exiv2free(edata);
			return NULL;
		}
		return edata;
	}
	catch (Exiv2::Error& e) {
		exiv2free(edata);
		return NULL;
	}
}

void exiv2free(struct exiv2data *edata){
	if(edata->imgused) edata->img->~Image();
	free(edata);
}

const char *datekeys[]={
	"Exif.Photo.DateTimeOriginal",
	"Exif.Image.DateTime",
	"Exif.Photo.DateTimeDigitized",
	NULL,
};

struct exeasy *edchkeasy(const char *key){
	struct exeasy *exy=exeasy;
	for(;exy->key;exy++) if(!strcmp(key,exy->key)) return exy;
	return NULL;
}

const char *edgeteasy(struct exeasy *exy,Exiv2::ExifData &ed){
	Exiv2::ExifData::const_iterator pos=exy->fnc(ed);
	return pos==ed.end()?"":pos->print(&ed).c_str();
}

char edchk(Exiv2::ExifData &ed,const char *key){
	if(!strncmp(key,"Easy.",5)) return 1;
	return ed[key].typeId()!=Exiv2::invalidTypeId;
}

void edgetsubstr(const char **rstr,int get){
	char *fstr=(char*)malloc(1024), *tstr=fstr;
	char *tok=NULL;
	snprintf(tstr,1024,"%s",*rstr);
	while(get>=0 && (tok=strsep(&tstr," \t"))) get--;
	if(tok) *rstr=tok;
	free(fstr);
}

void edonoffstr(const char **rstr){
	if(!strncmp(*rstr,"1",2)) *rstr="on";
	if(!strncmp(*rstr,"0",2)) *rstr="off";
}

char exiv2getstr(struct exiv2data *edata,const char **key,char *str,int len){
	Exiv2::ExifData &ed = edata->img->exifData();
	const char *delm=NULL;
	int num=0;
	int get=-1;
	char onoff=0;
	struct exeasy *easy;
	if(!strncmp(key[0],"join:",5))  delm=(key++)[0]+5;
	if(!strncmp(key[0],"get:",4))   get=atoi((key++)[0]+4);
	if(!strncmp(key[0],"onoff:",6)) onoff=1, key++;
	for(;key[0];key++) if((easy=edchkeasy(key[0])) || edchk(ed,key[0])){
		const char *rstr = easy ? edgeteasy(easy,ed) : ed[key[0]].print().c_str();
		if(!rstr || !rstr[0]) continue;
		if(get>=0) edgetsubstr(&rstr,get);
		if(onoff)  edonoffstr(&rstr);
		num+=snprintf(str+num,len-num,"%s%s",num && delm ? delm : "",rstr);
		if(!delm && num) return 1;
	}
	return num!=0;
}

long exiv2getint(struct exiv2data *edata,const char *key){
	Exiv2::ExifData &ed = edata->img->exifData();
	if(!edchk(ed,key)) return 0;
	return ed[key].toLong();
}

#endif
