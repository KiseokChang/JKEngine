#ifndef _BATTREC_H
#define _BATTREC_H

#include "posrec.h"

#define BATTSTAT_READY    1
#define BATTSTAT_FIRE     2
#define BATTSTAT_MOVE     3

class BattalionRecord : public PositionRecord
{
public:
	BattalionRecord();
 ~BattalionRecord();

  RecordBase *Duplicate();

	BOOL GetAZ(int32& az);
  BOOL SetAZ(int32 az);

	uint16 GetStatus();
	void SetStatus(uint16 status);

	BOOL GetBalsu(int32& balsu);
	BOOL SetBalsu(int32 balsu);

	BOOL GetMunsu(int32& munsu);
	BOOL SetMunsu(int32 munsu);

	WORD GetResID();
};
#endif