#ifndef _HOWIZMAN_H
#define _HOWIZMAN_H

#include "howizrec.h"
#include "dataman.h"

class HowitzerManager : public DataManager
{
public:
	HowitzerManager(char *fname="howitzer");
 ~HowitzerManager();

	HowitzerRecord *AddedRecord;
};

//////////////////////////////////////
extern HowitzerManager *HowitzerMan;//
//////////////////////////////////////

#endif