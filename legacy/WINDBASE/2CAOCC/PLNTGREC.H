#ifndef _PLNTGREC_H
#define _PLNTGREC_H

#include "tgrec.h"

class PlanTargetRecord : public RecordBase
{
public:
	PlanTargetRecord();
 ~PlanTargetRecord();

	RecordBase *Duplicate();

	BOOL GetBalsu(uint16 i,int32& balsu);
	BOOL GetStartH(uint16 i,int32& htime);
	BOOL GetEndH(uint16 i,int32& htime);
	BOOL GetIntervalH(uint16 i,uint32& interval);
	int16 GetType();

  BOOL GetTargetRecord(TargetRecord& tgrec);
};
#endif