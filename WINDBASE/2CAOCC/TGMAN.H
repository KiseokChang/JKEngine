#ifndef _TGMAN_H
#define _TGMAN_H

#include "tgrec.h"
#include "posdtman.h"

class TargetManager : public PositionDataManager
{
public:
	TargetManager(char *fname="tgman");
 ~TargetManager();

	TargetRecord *AddedRecord;
};

//////////////////////////////////
extern TargetManager *TargetMan;//
//////////////////////////////////

#endif