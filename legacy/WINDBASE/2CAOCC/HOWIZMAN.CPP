#include "howizman.h"

HowitzerManager::HowitzerManager(char *fname):
	DataManager()
{
	AddedRecord=new HowitzerRecord();
	if(!ChangeFileName(fname,*AddedRecord))
		CreationError=TRUE;
}

HowitzerManager::~HowitzerManager()
{}

///////////////////////////////
HowitzerManager *HowitzerMan=0;//
///////////////////////////////

