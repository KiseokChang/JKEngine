#ifndef _BOOLENT_H
#define _BOOLENT_H

#include "entbase.h"

class BooleanEntry : public EntryBase
{
public:
	BooleanEntry(WORD entid,int8 *name="BooleanEntry",BOOL init=FALSE);
 ~BooleanEntry();

  EntryBase *Duplicate();
	int16 Compare(EntryBase *ent,uint16);
	BOOL  SetData(BYTE *data);
	void  Get(FILE *fp);
	void  Put(FILE *fp);

	BOOL  SetBoolean(BOOL boolean);
	BOOL  GetBoolean();

	BOOL DataBoolean;
};
#endif