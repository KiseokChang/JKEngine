#include "tgrefent.h"

TargetRefEntry::TargetRefEntry(WORD entid,char *name) :
	RealPosition(),NumberEntry(entid,name,-1,10000,10,-1)
{
}
TargetRefEntry::~TargetRefEntry()
{}


EntryBase *TargetRefEntry::Duplicate()
{
	return CreateEqualEntry(new TargetRefEntry(EntryID,EntryName));
}

BOOL TargetRefEntry::GetCoord(JKPoint32& coord)
{
	TargetRecord tgrec;
	if(GetTargetRecord(tgrec)) return tgrec.GetCoord(coord);
	return FALSE;
}

BOOL TargetRefEntry::GetTargetRecord(TargetRecord& tgrec)
{
	int16	index=(int16)DataNumber;
	if(index>=0){
  	REFENTRY refentry;
		if(TargetMan->TableMan.GetRefEntry(index,refentry) && refentry.HasData){
			return TargetMan->RecordMan.GetRecord(index,tgrec);
		}
	}
	return FALSE;
}
