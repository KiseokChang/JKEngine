#ifndef _WANCODE_H
#define _WANCODE_H

#include "typedef.h"

#define NUMHANGUL 2350
#define SINGLEHAN 51

extern uint16 wCodeTable[NUMHANGUL];
extern uint16 SingleHan[SINGLEHAN];

BYTE16 KSSM2KS(BYTE16 ch);

#endif