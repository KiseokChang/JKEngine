#include <ctype.h>
#include "basefunc.h"
#include "boolent.h"

BooleanEntry::BooleanEntry(WORD entid,int8 *name,BOOL init):
	EntryBase(entid,name,5),DataBoolean(init)
{
	FileSize=sizeof(DataBoolean);
	SetBoolean(init);
}

BooleanEntry::~BooleanEntry()
{
}

EntryBase *BooleanEntry::Duplicate()
{
	return CreateEqualEntry(new BooleanEntry(EntryID,EntryName,DataBoolean));
}

int16 BooleanEntry::Compare(EntryBase *ent,uint16)
{
	int16 result=EntryBase::Compare(ent);
	if(!result){
		BooleanEntry *intent=(BooleanEntry *)ent;
		result = (DataBoolean==intent->DataBoolean)? 0:2;
	}
	return result;
}


BOOL BooleanEntry::SetData(BYTE *data)
{
	if(!data) return FALSE;
	if(!strcmp(data,"T")||!strcmp(data,"TRUE"))
		return SetBoolean(TRUE);
	else if(!strcmp(data,"F")||!strcmp(data,"FALSE"))
		return SetBoolean(FALSE);;
	return FALSE;
}

void BooleanEntry::Get(FILE *fp)
{
	if(fp) {
		BOOL boolean;
		fread(&boolean,sizeof(boolean),1,fp);
		SetBoolean(boolean);
	}
}

void BooleanEntry::Put(FILE *fp)
{
	if(fp) fwrite(&DataBoolean,sizeof(DataBoolean),1,fp);
}

BOOL BooleanEntry::SetBoolean(BOOL boolean)
{
	if(boolean) strcpy(DataBuffer,"TRUE ");
	else strcpy(DataBuffer,"FALSE");
	DataBoolean=boolean;
	return TRUE;
}

BOOL BooleanEntry::GetBoolean()
{
	return DataBoolean;
}


