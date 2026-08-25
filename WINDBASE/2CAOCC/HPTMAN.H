#ifndef _HPTMAN_H
#define _HPTMAN_H

#include "hptrec.h"
#include "posdtman.h"

class HPTManager : public PositionDataManager
{
public:
	HPTManager(char *fname="hptman");
 ~HPTManager();

	HPTRecord *AddedRecord;
};

/////////////////////
extern HPTManager *HPTMan;//
/////////////////////
#endif