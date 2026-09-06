#include "eq24name.h"
#include <dir.h>
#include <string.h>

Equip24Name::Equip24Name(char *fname):
	FilePoint(0),CreationError(FALSE),DFileMan()
{
	ffblk ffblk;
	InitFile(fname);
	if(findfirst(FileName,&ffblk,FA_ARCH)) DFileMan.FileCreate(FileName,sizeof(NAME24));
	FilePoint=DFileMan.FileOpen(FileName);
	if(FilePoint==-1) CreationError=TRUE;
}

Equip24Name::~Equip24Name()
{
	DFileMan.FileClose(FilePoint);
}

void Equip24Name::ChangeFileName(char *name)
{
	InitFile(name);
	FilePoint=DFileMan.ChangeFileName(name,sizeof(NAME24));
	if(FilePoint==-1) CreationError=TRUE;
	else CreationError=FALSE;
}

BOOL Equip24Name::GetRecord(RPTR index)
{
	if(CreationError) return FALSE;
	if(!DFileMan.IsUseFul(FilePoint,index)) return FALSE;
	if(DFileMan.GetRecord(FilePoint,index,(char *)&NameRecord,DFileMan.FileHeader[FilePoint].Record_Length)) return TRUE;
	return FALSE;
}

RPTR Equip24Name::PutRecord()
{
	if(CreationError) return -1;
	return DFileMan.NewRecord(FilePoint,(char *)&NameRecord);
}

BOOL Equip24Name::PutRecord(NAME24& buf,RPTR index)
{
	return DFileMan.PutRecord(FilePoint,index,(char *)&buf,DFileMan.FileHeader[FilePoint].Record_Length);
}


RPTR Equip24Name::DeleteRecord(RPTR index)
{
	if(!GetRecord(index)) return -1;
	DFileMan.DeleteRecord(FilePoint,index);
	return NameRecord.FirstIndex;
}

BOOL Equip24Name::GetRecord(NAME24& buf,RPTR index)
{
	if(CreationError) return FALSE;
	if(!DFileMan.IsUseFul(FilePoint,index)) return FALSE;
	if(DFileMan.GetRecord(FilePoint,index,(char *)&buf,DFileMan.FileHeader[FilePoint].Record_Length)) return TRUE;
	return FALSE;
}

RPTR Equip24Name::PutRecord(NAME24& buf)
{
	if(CreationError) return -1;
	return DFileMan.NewRecord(FilePoint,(char *)&buf);
}

RPTR Equip24Name::RecordingPosition()
{
	if(CreationError) return FALSE;
	return DFileMan.GetRecordingPosition(FilePoint);
}

void Equip24Name::InitFile(char *fname)
{
	if(fname) {
		int16 len=strlen(fname);
		if(len>12) len=12;
		strncpy(FileName,fname,len);
		FileName[12]=0;
	}
	else strcpy(FileName,EQP24NAME);
}

BOOL Equip24Name::Compare(NAME24& name1,char *buf,WORD compkind)
{
	BOOL rtv=FALSE;
	if(compkind==0x0000) return TRUE;
	if(compkind==0x0001&&!strcmp(name1.Division,buf)) rtv=TRUE;
	if(compkind==0x0002&&!strcmp(name1.Attached,buf)) rtv=TRUE;
	if(compkind==0x0004&&!strcmp(name1.Name,buf)) rtv=TRUE;
	if(compkind==0x0008&&!strcmp(name1.Number,buf)) rtv=TRUE;
	return rtv;
}

BOOL Equip24Name::Compare(NAME24& name1,NAME24& name2,WORD compkind)
{
	BOOL rtv=FALSE;
	if(compkind==0x0000) return TRUE;
	if(compkind&0x0001){
		if(!strcmp(name1.Division,name2.Division)) rtv=TRUE;
		else return FALSE;
	}
	if(compkind&0x0002){
		if(!strcmp(name1.Attached,name2.Attached)) rtv=TRUE;
		else return FALSE;
	}
	if(compkind&0x0004){
		if(!strcmp(name1.Name,name2.Name)) rtv=TRUE;
		else return FALSE;
	}
	if(compkind&0x0008){
		if(!strcmp(name1.Number,name2.Number)) rtv=TRUE;
		else return FALSE;
	}
	return rtv;
}

RPTR Equip24Name::Search(char *buf,RPTR index=0,WORD compkind)
{
	while(index<DFileMan.FileHeader[FilePoint].Record_Count) {
		if(GetRecord(WorkRecord,index)){
			if(Compare(WorkRecord,buf,compkind)) return index;
		}
		index++;
	}
	return -1;
}

RPTR Equip24Name::Search(NAME24& name2,RPTR index=0,WORD compkind)
{
	while(index<DFileMan.FileHeader[FilePoint].Record_Count) {
		if(GetRecord(WorkRecord,index)){
			if(Compare(WorkRecord,name2,compkind)) return index;
		}
		index++;
	}
	return -1;
}



