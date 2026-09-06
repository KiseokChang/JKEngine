#ifndef _REALENT_H
#define _REALENT_H

#include "entbase.h"

class RealEntry : public EntryBase
{
public:
	RealEntry(WORD entid,char *name="RealNumberEntry",double lowlimit=0.,double highlimit=10000.,double init=0.0);
 ~RealEntry();
	EntryBase *Duplicate();
	int16 Compare(EntryBase *ent,uint16 compkind=0xffff);
	BOOL SetData(BYTE *data);
	void Get(FILE *fp);
	void Put(FILE *fp);

	////////////////////////////////////////////////////////////

	BOOL IsValidNumber(double number);
	BOOL SetNumber(double number);
	BOOL GetNumber(double& number);

	double DataNumber;
	double LowLimit;
	double HighLimit;
};
#endif

