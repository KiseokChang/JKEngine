#ifndef _POSREC_H
#define _POSREC_H

#include "realpos.h"
#include "recbase.h"

class PositionRecord : public RecordBase , public RealPosition
{
public:
	PositionRecord(WORD recid,char *name="PositionRecord",WORD posentid=-1);
 ~PositionRecord();

	BOOL SetCoord(JKPoint32& p);
	BOOL GetCoord(JKPoint32& p);

	WORD PosEntryID;
};
#endif