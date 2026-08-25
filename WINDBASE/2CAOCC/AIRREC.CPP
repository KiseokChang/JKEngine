#include "calccord.h"
#include "dateent.h"
#include "timeent.h"
#include "coordent.h"
#include "numentry.h"
#include "airrec.h"

AirRecord::AirRecord():
	RecordBase(10,"AirRecord")
{
	HuboManager *huboman;

	AddEntry(new CoordEntry( 0,"¹ÁÎa1"));
	AddEntry(new CoordEntry( 1,"¹ÁÎa2"));
	AddEntry(new CoordEntry( 2,"¹ÁÎa3"));
	AddEntry(new CoordEntry( 3,"¹ÁÎa4"));

	AddEntry(new NumberEntry(4,"ÂA¸á‰¡•¡",0,100000));
	AddEntry(new NumberEntry(5,"ÂA‰¡‰¡•¡",0,100000));
  AddEntry(new NumberEntry(6,"¯¡¸b·©¸a",1,31));
  AddEntry(new TimeEntry(7,"¯¡¸b¯¡ˆe"));
  AddEntry(new NumberEntry(8,"{·©¸a",1,31));
  AddEntry(new TimeEntry(9,"{¯¡ˆe"));
}

AirRecord::~AirRecord()
{}

RecordBase *AirRecord::Duplicate()
{
	return CreateEqualRecord(new AirRecord(),TRUE);
}

BOOL AirRecord::SetCoord(uint16 i,JKPoint32 p)
{
	if(i<4){
		CoordEntry *ent=(CoordEntry *)GetEntry(i);
		if(ent) return ent->SetCoord(p);
	}
	return FALSE;
}

BOOL AirRecord::GetCoord(uint16 i,JKPoint32& p)
{
	if(i<4){
		CoordEntry *ent=(CoordEntry *)GetEntry(i);
		if(ent) return ent->GetCoord(p);
	}
	return FALSE;
}

BOOL AirRecord::SetCoords(JKPoint32 *p)
{
	if(!p) return FALSE;
	CoordEntry *ent;
	for(uint16 i=0;i<4;i++){
		ent=(CoordEntry *)GetEntry(i);
		if(ent) ent->SetCoord(p[i]);
	}
	return TRUE;
}

BOOL AirRecord::GetCoords(JKPoint32 p[4])
{
	if(!p) return FALSE;
	CoordEntry *ent;
	for(uint16 i=0;i<4;i++){
		ent=(CoordEntry *)GetEntry(i);
		if(ent) ent->GetCoord(p[i]);
	}
	return TRUE;
}

BOOL AirRecord::IsFirable(JKPoint32 p1,JKPoint32 p2,struct time& /*firetime*/)
{
	CoordCalculator coordcalc;
	JKPoint32 p[4];
	GetCoords(p);
	for(uint16 i=0;i<4;i++){
		if(coordcalc.IsCross(p1,p2,p[i],p[(i+1)%4]))
			return FALSE;
	}
	return TRUE;
}
