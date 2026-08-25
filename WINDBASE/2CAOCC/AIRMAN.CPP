#include "airman.h"

AirManager::AirManager(char *fname):
	DataManager()
{
	AddedRecord=new AirRecord();
	if(!ChangeFileName(fname,*AddedRecord))
		CreationError=TRUE;
}

AirManager::~AirManager()
{}

///////////////////////
AirManager *AirMan=0;//
///////////////////////
