#ifndef _POSDTMAN_H
#define _POSDTMAN_H

#include "posrec.h"
#include "dataman.h"

class PositionDataManager : public DataManager
{
public:
	PositionDataManager();
 ~PositionDataManager();

	int16 SearchNearCoord(JKPoint32 p,uint32 delta,int16 findstart=-1);
};
#endif