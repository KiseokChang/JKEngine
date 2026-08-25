#ifndef _256PCX_H
#define _256PCX_H

#include "jkprog.h"

BOOL SkipPCXLine(FILE *fp,unsigned bytes);
unsigned ReadPCXLine(FILE *fp,char *s,int bytes,BOOL palchange=TRUE);
BOOL PutPCX(char *fname,JKPoint p,JKRect rect,BYTE putmode=0,BOOL palchange=TRUE);
#endif