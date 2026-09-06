#include <stdlib.h>
#include <ctype.h>
#include "numentry.h"

NumberEntry::NumberEntry(WORD entid,char *name,int32 lowlimit,int32 highlimit,uint16 radix,int32 init):
	EntryBase(entid,name),DataNumber(0)
{
	FileSize=sizeof(DataNumber);

	SetDataForm(lowlimit,highlimit,radix,init);
}

NumberEntry::~NumberEntry()
{}


void NumberEntry::SetDataForm(int32 lowlimit,int32 highlimit,uint16 radix,int32 init)
{
	LowLimit=lowlimit;
	HighLimit=highlimit;
	Radix=radix;

	if(Radix<2 || Radix>36) Radix=2;

	if(LowLimit>HighLimit){
		int32 number=HighLimit;
		HighLimit=LowLimit;
		LowLimit=number;
	}
	if(init<LowLimit || init>HighLimit) init=LowLimit;

	char lowbuffer[32+1],highbuffer[32+1];
	ltoa(LowLimit,lowbuffer,Radix);
	ltoa(HighLimit,highbuffer,Radix);
	int32 lowlen  = strlen(lowbuffer);
	int32 highlen = strlen(highbuffer);
	BufferSize=max(lowlen,highlen);

	if(SpaceBuffer) delete[] SpaceBuffer;
	if(DataBuffer) delete[] DataBuffer;
	SpaceBuffer = new BYTE[BufferSize+1];
	ltoa(init,lowbuffer,Radix);
	if(SpaceBuffer)	CopyDataBuffer(SpaceBuffer,lowbuffer,BufferSize);
	DataBuffer = new BYTE[BufferSize+1];
	if(DataBuffer) DataBuffer[BufferSize]=0;

	SetNumber(init);
}
////////////////////////////////////////////////////////////////////////
EntryBase *NumberEntry::Duplicate()
{
	return CreateEqualEntry(new NumberEntry(EntryID,EntryName,LowLimit,HighLimit,Radix));
}

int16 NumberEntry::Compare(EntryBase *ent,uint16)
{
	int16 result=EntryBase::Compare(ent);
	if(!result){
		NumberEntry *nument=(NumberEntry *)ent;
		result = (DataNumber>nument->DataNumber)? 1:(DataNumber<nument->DataNumber)? -1:0;
	}
	return result;
}

BOOL NumberEntry::SetData(BYTE *data)
{
	if(!data) return FALSE;
	int32 number=atol(data);
	return SetNumber(number);
}

void NumberEntry::Get(FILE *fp)
{
	if(fp) {
		int32 number;
		fread(&number,sizeof(number),1,fp);
		SetNumber(number);
	}
}

void NumberEntry::Put(FILE *fp)
{
	if(fp) fwrite(&DataNumber,sizeof(DataNumber),1,fp);
}


/////////////////////////////////////////////////////////

BOOL NumberEntry::IsValidNumber(int32 number)
{
	if(number>=LowLimit && number<=HighLimit) return TRUE;
	return FALSE;
}

BOOL NumberEntry::SetNumber(int32 number)
{
	if(IsValidNumber(number)){
		char buffer[32+1];
		ltoa(number,buffer,Radix);
		if(EntryBase::SetData(buffer)){
			DataNumber=number;
			return TRUE;
		}
	}
	return FALSE;
}

BOOL NumberEntry::GetNumber(int32& number)
{
	if(IsValidNumber(DataNumber)){
		number=DataNumber;
		return TRUE;
	}
	return FALSE;
}
///////////////////////////////////////////////////////////
