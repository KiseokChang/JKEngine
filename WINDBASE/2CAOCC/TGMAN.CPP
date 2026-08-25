#include "tgman.h"

TargetManager::TargetManager(char *fname):
	PositionDataManager()
{
	AddedRecord=new TargetRecord();
	if(!ChangeFileName(fname,*AddedRecord))
		CreationError=TRUE;
}

TargetManager::~TargetManager()
{}

///////////////////////////
TargetManager *TargetMan=0;//
///////////////////////////

