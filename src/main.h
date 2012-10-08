#ifndef _MAIN_H
#define _MAIN_H

#define FILELEN			1024
#define LDFILELEN		20480

#ifndef __GNUC__
	#define  __attribute__(x)
#endif

#define UNUSED(x) UNUSED_ ## x __attribute__((unused)) 
#define FORMAT(a,b) __attribute__((format(printf,a,b)))
#define NORETURN __attribute__((noreturn))

#define ETIMER  E(NONE), E(SDL), E(DPL), E(GLLD), E(LD), E(LDF), E(COL), E(THR)
#define E(X)	TI_##X
enum timer { ETIMER };
#undef E

void timer(enum timer timer,int id,char reset);

#define EDEBUG	E(NONE), E(STA), E(DBG), E(EFF)
#define E(X)	DBG_##X
enum debug { EDEBUG, ERR_QUIT, ERR_CONT };
#undef E


#define THR_SDL 0x02
#define THR_DPL 0x04
#define THR_LD  0x08
#define THR_ACT 0x10
#define THR_ML1 0x20
#define THR_ML2 0x40
#define THR_OTH (THR_DPL|THR_LD|THR_ACT|THR_ML1)
//#define THR_OTH (THR_DPL|THR_LD|THR_ACT|THR_ML1|THR_ML2)

#define debug(lvl,...)	debug_ex(lvl,__FILE__,__LINE__,__VA_ARGS__)
#define error(lvl,...)	debug_ex(lvl,__FILE__,__LINE__,__VA_ARGS__)

void debug_ex(enum debug lvl,const char *file,int line,const char *txt,...) FORMAT(4,5);

int mprintf(const char *format,...) FORMAT(1,2);

const char *getprogpath();
char *finddatafile(const char *fn);
const char *gettmp();

int threadid();
size_t nthreads();

#endif
