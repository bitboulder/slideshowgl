#ifndef _MAIN_H
#define _MAIN_H

#define EDEBUG	E(NONE), E(STA), E(DBG)
#define E(X)	DBG_##X
enum debug { EDEBUG, ERR_QUIT, ERR_CONT };
#undef E


#define THR_SDL 0x1
#define THR_DPL 0x2
#define THR_LD  0x4
#define THR_OTH 0x6

#define debug(lvl,...)	debug_ex(lvl,__FILE__,__LINE__,__VA_ARGS__)
#define error(lvl,...)	debug_ex(lvl,__FILE__,__LINE__,__VA_ARGS__)

void debug_ex(enum debug lvl,char *file,int line,char *txt,...);

#endif
