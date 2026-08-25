#include "xbattman.h"

XBattalionManager::XBattalionManager(char *fname):
	PositionDataManager()
{
	AddedRecord=new XBattalionRecord();
	if(!ChangeFileName(fname,*AddedRecord))
		CreationError=TRUE;
}

XBattalionManager::~XBattalionManager()
{}

////////////////////////////////////
XBattalionManager  *XBattalionMan=0;//
////////////////////////////////////

