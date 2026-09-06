#ifndef _HPTREC_H
#define _HPTREC_H

#include "tgrec.h"

class HPTRecord : public RecordBase
{
public:
	HPTRecord();
 ~HPTRecord();

	RecordBase *Duplicate();

	BOOL GetTargetRecord(TargetRecord& tgrec);
};
#endif