#include "battman.h"

BattalionManager::BattalionManager(char *fname):
	PositionDataManager()
{
	AddedRecord=new BattalionRecord();
	if(!ChangeFileName(fname,*AddedRecord))
		CreationError=TRUE;
}

BattalionManager::~BattalionManager()
{}

//////////////////////////////////
BattalionManager  *BattalionMan=0;//
//////////////////////////////////

