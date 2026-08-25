#ifndef _AirMAN_H
#define _AirMAN_H

#include "airrec.h"
#include "dataman.h"

class AirManager : public DataManager
{
public:
	AirManager(char *fname="airman");
 ~AirManager();

	AirRecord *AddedRecord;
};

extern AirManager *AirMan;
#endif