#ifndef _NUMENTRY_H
#define _NUMENTRY_H

#include "entbase.h"

class NumberEntry : public EntryBase
{
public:
	NumberEntry(WORD entid,char *name="NumberEntry",int32 lowlimit=0,int32 highlimit=100000,uint16 radix=10,int32 init=0);
 ~NumberEntry();

	void SetDataForm(int32 lowlimit=0,int32 highlimit=100000,uint16 radix=10,int32 init=0);

	EntryBase *Duplicate();

	int16 Compare(EntryBase *ent,uint16 compkind=0xffff);
	BOOL SetData(BYTE *data);
	void Get(FILE *fp);
	void Put(FILE *fp);
	BOOL IsValidNumber(int32 number);
	BOOL SetNumber(int32 number);
	BOOL GetNumber(int32& number);

	int32 DataNumber;
	int32 LowLimit;
	int32 HighLimit;
  uint16 Radix;
};
#endif