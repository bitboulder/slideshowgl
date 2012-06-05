#include <stdlib.h>
#include <stdio.h>
#include "ldcp.h"
#include "img.h"
#include "help.h"
#include "main.h"
#include "dpl.h"

struct loadconcept loadconcepts[] = {
	{ "F:0,B:0..2,-1;S:+-5,+-10,3..4,-2..-4", "F:0,B:-2..4;S:..15;T:..24" }, /* zoom >  0 */
	{ "B:0..2,-1;S:+-5,+-10,3..4,-2..-4",     "F:0,B:-2..4;S:..15;T:..24" }, /* zoom =  0 */
	{ "S:..17;B:0,1",                         "F:0,B:-1..2;S:..22;T:..38" }, /* zoom = -1 */
	{ "S:..22;T:+-23..31;B:0",                "B:..1;S:..37;T:..52"       }, /* zoom = -2 */
	{ "T:..38;S:..22;B:0",                    "B:..1;S:..22;T:..59"       }, /* zoom = -3 */
	{ "T:..58;B:0",                           "B:..1;T:..83"              }, /* zoom = -4 */
	{ "T:..82;B:0",                           "B:..1;T:..111"             }, /* zoom = -5 */
	{ "T:..110;B:0",                          "B:..1;T:..143"             }, /* zoom = -6 */
	{ "T:..142;B:0",                          "B:..1;T:..179"             }, /* zoom = -7 */
};
#define NUM_CONCEPTS (sizeof(loadconcepts)/sizeof(struct loadconcept))

struct loadconept_ld *ldconceptadd(int imgi,enum imgtex tex){
	static unsigned int num=0,size=0;
	static struct loadconept_ld *ret=NULL;

	if(num==size) ret=realloc(ret,sizeof(struct loadconept_ld)*(size+=32));
	ret[num].imgi=imgi;
	ret[num].tex=tex;
	num++;

	if(tex==TEX_NONE){
		struct loadconept_ld *retl=ret;
		num=size=0;
		ret=NULL;
		return retl;
	}else return NULL;
}

struct loadconept_ld *ldconceptstr(const char *str){
	char strcopy[1024];
	char *pos;
	char *tok;
	enum imgtex tex=TEX_NONE;
	snprintf(strcopy,1024,str);
	pos=strcopy;
	while((tok=strsep(&pos,",;"))){
		char sign=0;
		int  istr,iend;
		char *tokend;
		if(tok[1]==':'){
			switch(tok[0]){
				case 'F': tex=TEX_FULL; break;
				case 'B': tex=TEX_BIG; break;
				case 'S': tex=TEX_SMALL; break;
				case 'T': tex=TEX_TINY; break;
				default: error(ERR_QUIT,"concept: unknown tex '%c'",tok[0]);
			}
			tok+=2;
		}
		if(tok[0]=='.' && tok[1]=='.'){
			iend=(int)strtol(tok+2,&tokend,0);
			if(tokend==tok+2) error(ERR_QUIT,"concept: no digits after '..': '%s'",tokend);
			if(tokend[0]) error(ERR_QUIT,"concept: str after '..[0-9]+': '%s'",tokend);
			if(iend<0) iend*=-1;
			for(istr=0;istr<=iend;istr++){
				ldconceptadd(istr,tex);
				ldconceptadd(-istr,tex);
			}
			continue;
		}
		if(tok[0]=='+' && tok[1]=='-'){ sign=1; tok+=2; }
		istr=(int)strtol(tok,&tokend,0);
		if(tokend==tok) error(ERR_QUIT,"concept: no digits at begin: '%s'",tokend);
		if(tokend[-1]=='.') tokend--;
		tok=tokend;
		if(tok[0]=='.' && tok[1]=='.'){
			iend=(int)strtol(tok+2,&tokend,0);
			if(tokend==tok+2) error(ERR_QUIT,"concept: no digits after '[0-9]+..': '%s'",tokend);
			if(tokend[0]) error(ERR_QUIT,"concept: str after '[0-9]+..[0-9]+': '%s'",tokend);
		}else{
			if(tok[0]) error(ERR_QUIT,"concept: str after '[0-9]+': '%s' (%s)",tok,str);
			iend=istr;
		}
		while(1){
			ldconceptadd(istr,tex);
			if(sign) ldconceptadd(-istr,tex);
			if(istr==iend) break;
			istr += istr<iend ? 1 : -1;
		}
	}
	return ldconceptadd(0,TEX_NONE);
}

void ldconceptcompile(){
	unsigned int z;
	for(z=0;z<NUM_CONCEPTS;z++){
		int i;
		int hmin,hmax;
		struct loadconept_ld *hold;
		loadconcepts[z].load = ldconceptstr(loadconcepts[z].load_str);
		hold = ldconceptstr(loadconcepts[z].hold_str);
		for(hmin=hmax=i=0;hold[i].tex!=TEX_NONE;i++){
			if(hold[i].imgi<hmin) hmin=hold[i].imgi;
			if(hold[i].imgi>hmax) hmax=hold[i].imgi;
		}
		loadconcepts[z].hold_min = hmin;
		loadconcepts[z].hold_max = hmax;
		loadconcepts[z].hold=malloc(sizeof(enum imgtex)*(long unsigned int)(hmax-hmin+1));
		memset(loadconcepts[z].hold,-1,sizeof(enum imgtex)*(long unsigned int)(hmax-hmin+1));
		for(i=0;hold[i].tex!=TEX_NONE;i++) if(loadconcepts[z].hold[hold[i].imgi-hmin]<hold[i].tex)
			loadconcepts[z].hold[hold[i].imgi-hmin]=hold[i].tex;
		free(hold);
	}
}

void ldconceptfree(){
	unsigned int z;
	for(z=0;z<NUM_CONCEPTS;z++){
		free(loadconcepts[z].load);
		free(loadconcepts[z].hold);
	}
}

struct loadconcept *ldconceptget(){
	int zoom = dplgetzoom();
	if(zoom>0) return loadconcepts+0;
	if(zoom==0) return loadconcepts+1;
	if((unsigned int)(1-zoom)<NUM_CONCEPTS) return loadconcepts+(1-zoom);
	return loadconcepts+NUM_CONCEPTS-1;
}
