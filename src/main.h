#ifndef _MAIN_H
#define _MAIN_H

enum debug { DBG_NONE, DBG_STA };
#define THR_SDL 0x1
#define THR_DPL 0x2
#define THR_LD  0x4
#define THR_OTH 0x6

void debug(enum debug lvl,char *txt,...);
void error(char quit,char *txt,...);

#endif
