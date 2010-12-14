#ifndef _MAIN_H
#define _MAIN_H

#define FILELEN		1024

#define ETIMER  E(NONE), E(SDL), E(DPL), E(LD), E(COL)
#define E(X)	TI_##X
enum timer { ETIMER };
#undef E

void timer(enum timer timer,int id,char reset);

#define EDEBUG	E(NONE), E(STA), E(DBG)
#define E(X)	DBG_##X
enum debug { EDEBUG, ERR_QUIT, ERR_CONT };
#undef E


#define THR_SDL 0x02
#define THR_DPL 0x04
#define THR_LD  0x08
#define THR_ACT 0x10
#define THR_OTH (THR_DPL|THR_LD|THR_ACT)

#define debug(lvl,...)	debug_ex(lvl,__FILE__,__LINE__,__VA_ARGS__)
#define error(lvl,...)	debug_ex(lvl,__FILE__,__LINE__,__VA_ARGS__)

void debug_ex(enum debug lvl,char *file,int line,char *txt,...);

char *finddatafile(char *fn);

#endif
