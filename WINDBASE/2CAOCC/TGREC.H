#ifndef _TGREC_H
#define _TGREC_H

#include "posrec.h"

class TargetRecord : public PositionRecord
{
public:
	TargetRecord();
 ~TargetRecord();

	RecordBase *Duplicate();

  WORD GetResID();
};
#endif