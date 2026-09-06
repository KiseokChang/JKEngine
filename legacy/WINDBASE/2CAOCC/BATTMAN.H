#ifndef _BATTMAN_H
#define _BATTMAN_H

#include "battrec.h"
#include "posdtman.h"

class BattalionManager : public PositionDataManager
{
public:
	BattalionManager(char *fname="battman");
 ~BattalionManager();

	BattalionRecord *AddedRecord;
};

/////////////////////////////////////////
extern BattalionManager  *BattalionMan;//
/////////////////////////////////////////

#endif