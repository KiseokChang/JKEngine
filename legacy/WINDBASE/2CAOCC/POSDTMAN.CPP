#include "posdtman.h"

PositionDataManager::PositionDataManager():
	DataManager()
{}

PositionDataManager::~PositionDataManager()
{}

int16 PositionDataManager::SearchNearCoord(JKPoint32 p,uint32 delta,int16 findstart)
{
	REFENTRY refentry;
	int16 index=-1;
	if(findstart<0) {
		index=TableMan.TableHead;
	}
	else{
		if(TableMan.GetRefEntry(findstart,refentry)&&refentry.HasData)
			index=refentry.next;
	}
	while(index!=-1&&TableMan.GetRefEntry(index,refentry)){
		if(refentry.HasData) {
			RecordMan.GetRecord(index,*WorkRecord);
			if(((PositionRecord *)WorkRecord)->IsNearCoord(p,delta))	return index;
			index=refentry.next;
		}
		else index=-1;
	}
	return -1;
}