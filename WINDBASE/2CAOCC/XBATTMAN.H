#ifndef _XBATTMAN_H
#define _XBATTMAN_H

#include "xbattrec.h"
#include "posdtman.h"

class XBattalionManager : public PositionDataManager
{
public:
	XBattalionManager(char *fname="xbattman");
 ~XBattalionManager();

	XBattalionRecord *AddedRecord;
};

///////////////////////////////////////////
extern XBattalionManager  *XBattalionMan;//
///////////////////////////////////////////

#endif