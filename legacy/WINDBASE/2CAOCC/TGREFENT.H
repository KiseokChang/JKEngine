#ifndef _TGREFENT_H
#define _TGREFENT_H

#include "realpos.h"
#include "numentry.h"

#include "tgman.h"

class TargetRefEntry : public RealPosition,public NumberEntry
{
public:
	TargetRefEntry(WORD entid,char *name="TargetRefEntry");
 ~TargetRefEntry();

	EntryBase *Duplicate();
	BOOL GetCoord(JKPoint32& coord);

	BOOL GetTargetRecord(TargetRecord& tgrec);
};

#endif
