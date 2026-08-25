#include "coordent.h"
#include "posrec.h"

PositionRecord::PositionRecord(WORD recid,char *name,WORD posentid):
	RecordBase(recid,name),RealPosition(),PosEntryID(posentid)
{}

PositionRecord::~PositionRecord()
{}

BOOL PositionRecord::SetCoord(JKPoint32& p)
{
	CoordEntry *ent=(CoordEntry *)GetEntry(PosEntryID);
	if(ent) return ent->SetCoord(p);
	return FALSE;
}


BOOL PositionRecord::GetCoord(JKPoint32& p)
{
	CoordEntry *ent=(CoordEntry *)GetEntry(PosEntryID);
	if(ent) return ent->GetCoord(p);
	return FALSE;
}


