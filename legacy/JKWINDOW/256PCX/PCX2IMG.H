#ifndef _PCX2IMG_H
#define _PCX2IMG_H

#include "jkprog.h"

uint16 ReadPCXLine(FILE *fp,char *s,int16 bytes,BOOL palchange=FALSE);
BOOL PCX2IMG(char *pcxfn,char *imgfn);
#endif