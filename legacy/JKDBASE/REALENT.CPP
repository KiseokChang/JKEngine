#include <stdlib.h>
#include <ctype.h>
#include "realent.h"

RealEntry::RealEntry(WORD entid,char *name,double lowlimit,double highlimit,double init):
	EntryBase(entid,name,11),LowLimit(lowlimit),HighLimit(highlimit),DataNumber(0.0)
{
	FileSize=sizeof(DataNumber);

	if(LowLimit>HighLimit){
		double number=HighLimit;
		HighLimit=LowLimit;
		LowLimit=number;
	}

	if(init<LowLimit || init>HighLimit) init=LowLimit;

	SetNumber(init);
}

RealEntry::~RealEntry()
{}

EntryBase *RealEntry::Duplicate()
{
	return CreateEqualEntry(new RealEntry(EntryID,EntryName,LowLimit,HighLimit));
}

int16 RealEntry::Compare(EntryBase *ent,uint16)
{
	int16 result=EntryBase::Compare(ent);
	if(!result){
		RealEntry *realent=(RealEntry *)ent;
		result = (DataNumber>realent->DataNumber)? 1:(DataNumber<realent->DataNumber)? -1:0;
	}
	return result;
}

BOOL RealEntry::SetData(BYTE *data)
{
	if(!data) return FALSE;
	double number=atof(data);
	return SetNumber(number);
}

void RealEntry::Get(FILE *fp)
{
	if(fp) {
		double number;
		fread(&number,sizeof(number),1,fp);
		SetNumber(number);
	}
}

void RealEntry::Put(FILE *fp)
{
	if(fp) fwrite(&DataNumber,sizeof(DataNumber),1,fp);
}


////////////////////////////////////////////////////////////

BOOL RealEntry::IsValidNumber(double number)
{
	if(number>=LowLimit && number<=HighLimit) return TRUE;
	return FALSE;
}

BOOL RealEntry::SetNumber(double number)
{
	if(IsValidNumber(number)){
		char buffer[11+1];
		sprintf(buffer,"%-7.2f",number);
		if(EntryBase::SetData(buffer)){
			DataNumber=number;
			return TRUE;
		}
	}
	return FALSE;
}

BOOL RealEntry::GetNumber(double& number)
{
	if(IsValidNumber(DataNumber)){
		number=DataNumber;
		return TRUE;
	}
	return FALSE;
}

