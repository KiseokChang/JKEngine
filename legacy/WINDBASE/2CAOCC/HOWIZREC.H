#ifndef _HOWIZREC_H
#define _HOWIZREC_H

#include "recbase.h"

class HowitzerRecord : public RecordBase
{
public:
	HowitzerRecord();
 ~HowitzerRecord();

 	RecordBase *Duplicate();

	BOOL GetDistance(int32& nomal,int32& rap);
	BOOL GetLimit(int32& leftlimit,int32& rightlimit);
};
#endif