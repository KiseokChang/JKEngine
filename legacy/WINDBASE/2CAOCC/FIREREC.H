#ifndef _FIREREC_H
#define _FIREREC_H

#include "posrec.h"

class FireRecord : public PositionRecord
{
public:
	FireRecord();
 ~FireRecord();

	RecordBase *Duplicate();

	BOOL GetBalsu(int32& balsu);
	BOOL SetBalsu(int32 balsu);
};
#endif