#include <mem.h>
#include "plntgman.h"

PlanTargetManager::PlanTargetManager(char *fname):
	PositionDataManager()
{
	AddedRecord=new PlanTargetRecord();
	if(ChangeFileName(fname,*AddedRecord)){
		for(uint16 i=0;i<6;i++){
			EnableSet[i]=FALSE;
			setmem(&HTimeSet[i],6,0);
			setmem(&HLastTimeSet[i],6,0);
		}
	}
	else CreationError=TRUE;

}

PlanTargetManager::~PlanTargetManager()
{}

///////////////////////////////////
PlanTargetManager *PlanTargetMan=0;//
///////////////////////////////////

