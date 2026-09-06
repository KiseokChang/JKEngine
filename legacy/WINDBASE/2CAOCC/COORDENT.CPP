#include "coordent.h"

CoordEntry::CoordEntry(WORD entid,int8 *name,JKPoint32 init):
	RealPosition(),EntryBase(entid,name,10),DataCoord(0,0)
{
	FileSize=sizeof(DataCoord);
	SetCoord(init);
  strcpy(SpaceBuffer,"CT00000000");
}

CoordEntry::~CoordEntry()
{}

EntryBase *CoordEntry::Duplicate()
{
	return CreateEqualEntry(new CoordEntry(EntryID,EntryName));
}

int16 CoordEntry::Compare(EntryBase *ent,uint16)
{
	int16 result=EntryBase::Compare(ent);
	if(!result){
		CoordEntry *coordent=(CoordEntry *)ent;
		result = (DataCoord==coordent->DataCoord)? 0:2;
	}
	return result;
}


BOOL CoordEntry::SetData(BYTE *data)
{
	if(!data) return FALSE;
	JKPoint32 coord;
	if(RealCoordManager().StrToReal(data,coord))
		return SetCoord(coord);
	return FALSE;
}

BOOL CoordEntry::GetData(BYTE *data)
{
	return EntryBase::GetData(data);
}

void CoordEntry::Get(FILE *fp)
{
	if(fp) {
		JKPoint32 coord;
		fread(&coord,sizeof(coord),1,fp);
		SetCoord(coord);
	}
}

void CoordEntry::Put(FILE *fp)
{
	if(fp) fwrite(&DataCoord,sizeof(DataCoord),1,fp);
}

///////////////////////////////////////////////////////////

BOOL CoordEntry::SetCoord(JKPoint32& coord)
{
	RealCoordManager coordman;
	if(coordman.IsValidReal(coord)){
		char buffer[11];
		coordman.RealToStr(buffer,coord);
		if(EntryBase::SetData(buffer)){
			DataCoord=coord;
			return TRUE;
		}
	}
	return FALSE;
}

BOOL CoordEntry::GetCoord(JKPoint32& coord)
{
	if(IsValidCoord(DataCoord)){
		coord=DataCoord;
		return TRUE;
	}
	return FALSE;
}

