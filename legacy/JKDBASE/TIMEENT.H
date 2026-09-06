#ifndef _TIMEENT_H
#define _TIMEENT_H

#include "entbase.h"

class TimeEntry:public EntryBase
{
public:
	TimeEntry(WORD entid,char *name="TimeEntry",struct time *reftime=0);

	EntryBase *Duplicate();
	BOOL IsValidData(BYTE *data);
	int16 Compare(EntryBase *ent,uint16 compkind=0xffff);

	void SetCurrentTime();
	BYTE SetTime(struct time& reftime);
	BYTE StrToTime(int8 *str,struct time& reftime);
	BYTE TimeToStr(int8 *str,struct time& reftime);
	BYTE ValidTime(struct time& reftime);
};

#endif