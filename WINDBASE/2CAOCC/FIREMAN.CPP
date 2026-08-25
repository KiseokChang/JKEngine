#include "fireman.h"

FireManager::FireManager(char *fname):
	PositionDataManager()
{
	AddedRecord=new FireRecord();
	if(!ChangeFileName(fname,*AddedRecord))
		CreationError=TRUE;
}

FireManager::~FireManager()
{}

//////////////////////////////
FireManager *FireMan=0;     //
FireManager *PreFireMan=0;  //
FireManager *TempFireMan=0; //
//////////////////////////////

