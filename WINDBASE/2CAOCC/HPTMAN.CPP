#include "hptman.h"

HPTManager::HPTManager(char *fname):
	PositionDataManager()
{
	AddedRecord=new HPTRecord();
	if(!ChangeFileName(fname,*AddedRecord))
		CreationError=TRUE;
}

HPTManager::~HPTManager()
{
}

/////////////////////
HPTManager *HPTMan=0;//
/////////////////////
