#ifndef _PLNTGMAN_H
#define _PLNTGMAN_H

#include <dos.h>
#include "plntgrec.h"
#include "posdtman.h"

class PlanTargetManager : public PositionDataManager
{
public:
	PlanTargetManager(char *fname="plantarg");
 ~PlanTargetManager();

	PlanTargetRecord *AddedRecord;

	BOOL EnableSet[6];
	struct time HTimeSet[6];
	struct time HLastTimeSet[6];
};

///////////////////////////////////
extern PlanTargetManager *PlanTargetMan;//
///////////////////////////////////

#endif