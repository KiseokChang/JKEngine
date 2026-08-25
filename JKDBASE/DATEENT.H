#ifndef _DATEENT_H
#define _DATEENT_H

#include "entbase.h"

class DateEntry:public EntryBase
{
public:
	DateEntry(WORD entid,int8 *name="DateEntry",struct date *refdate=0);

	EntryBase *Duplicate();
	BOOL IsValidData(BYTE *data);
	int16 Compare(EntryBase *ent,uint16 compkind=0xffff);

	void SetCurrentDay();
	BYTE SetDate(struct date& refdate);
	BYTE StrToDate(int8 *str,struct date& refdate);
	BYTE DateToStr(int8 *str,struct date& refdate);
	BYTE ValidDate(struct date& refdate);
};

#endif