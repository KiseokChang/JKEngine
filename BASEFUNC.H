#ifndef _BASEFUNC_H
#define _BASEFUNC_H

#include "keydef.h"

void DeleteWhiteSpace(char *str);
char * StrChrNot(char *str,char ch);
char * StrChrNotReverse(char *str,char ch);
BOOL StrMaskPicture(char *str,char *picture,char mask);
BOOL StrMaskNotPicture(char *str,char *picture,char mask);

#define ADJUSTRIGHT		0
#define ADJUSTLEFT		1

void AdjustString(char *str,BYTE adjflag);

/////////////////////////////////////////////////////

template <class T>
int32 SearchItem(T *itemarray,uint32 itemcount,T& searchitem)
{
	if(!itemarray || !itemcount) return -1;

	for(uint32 i=0;i<itemcount;i++)
		if(itemarray[i]==searchitem) return i;
	return -1;
}

#endif