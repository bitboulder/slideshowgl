#include "config.h"
#if HAVE_EXIV2

#include <exiv2/image.hpp>
#include <exiv2/exif.hpp>
#include <exiv2/canonmn.hpp>
#include <exiv2/minoltamn.hpp>
#include <sstream>
#include <string>
#include <stdio.h>

#include "exiv2.h"

struct exiv2data {
	Exiv2::Image::AutoPtr img;
};

struct exiv2data *exiv2init(const char *fn){
	struct exiv2data *edata=(struct exiv2data *)calloc(1,sizeof(struct exiv2data));
	try {
	    edata->img = Exiv2::ImageFactory::open(fn);
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
	edata->img->~Image();
	free(edata);
}

const char *datekeys[]={
	"Exif.Photo.DateTimeOriginal",
	"Exif.Image.DateTime",
	"Exif.Photo.DateTimeDigitized",
	NULL,
};

char edchk(Exiv2::ExifData &ed,const char *key){
	return ed[key].typeId()!=Exiv2::invalidTypeId;
}

char exiv2getstr(struct exiv2data *edata,const char **key,char *str,int len){
	Exiv2::ExifData &ed = edata->img->exifData();
	const char *delm=NULL;
	int num=0;
	if(!strncmp(key[0],"join:",5)) delm=(key++)[0]+5;
	for(;key[0];key++) if(edchk(ed,key[0])){
		std::stringstream sstr;
		sstr << ed[key[0]];
		if(sstr.str().length()<=0) continue;
		num+=snprintf(str+num,len-num,"%s%s",num && delm ? delm : "",sstr.str().c_str());
		if(!delm && num) return 1;
	}
	return num!=0;
}

int exiv2getint(struct exiv2data *edata,const char *key){
	Exiv2::ExifData &ed = edata->img->exifData();
	if(!edchk(ed,key)) return 0;
	return ed[key].toLong();
}

#endif
