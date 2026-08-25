#ifndef _FIREMAN_H
#define _FIREMAN_H

#include "firerec.h"
#include "posdtman.h"

class FireManager : public PositionDataManager
{
public:
	FireManager(char *fname="firerslt");
 ~FireManager();

	FireRecord *AddedRecord;
};

///////////////////////////////////
extern FireManager *FireMan;     //
extern FireManager *PreFireMan;  //
extern FireManager *TempFireMan; //
///////////////////////////////////

#endif