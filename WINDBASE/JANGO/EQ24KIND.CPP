#include "eq24kind.h"
#include <dir.h>
#include <string.h>

Equip24Kind::Equip24Kind(char *fname):
	FilePoint(0),CreationError(FALSE),DFileMan()
{
	ffblk ffblk;
	InitFile(fname);
	if(findfirst(FileName,&ffblk,FA_ARCH)) DFileMan.FileCreate(FileName,sizeof(KIND24));
	FilePoint=DFileMan.FileOpen(FileName);
	if(FilePoint==-1) CreationError=TRUE;
}

Equip24Kind::~Equip24Kind()
{
	DFileMan.FileClose(FilePoint);
}

void Equip24Kind::ChangeFileName(char *name)
{
	InitFile(name);
	FilePoint=DFileMan.ChangeFileName(name,sizeof(KIND24));
	if(FilePoint==-1) CreationError=TRUE;
	else CreationError=FALSE;
}

BOOL Equip24Kind::GetRecord(RPTR index)
{
	if(CreationError) return FALSE;
	if(!DFileMan.IsUseFul(FilePoint,index)) return FALSE;
	if(DFileMan.GetRecord(FilePoint,index,(char *)&KindRecord,DFileMan.FileHeader[FilePoint].Record_Length)) return TRUE;
	return FALSE;
}

RPTR Equip24Kind::PutRecord()
{
	if(CreationError) return -1;
	return DFileMan.NewRecord(FilePoint,(char *)&KindRecord);
}

BOOL Equip24Kind::PutRecord(KIND24& buf,RPTR index)
{
	return DFileMan.PutRecord(FilePoint,index,(char *)&buf,DFileMan.FileHeader[FilePoint].Record_Length);
}

BOOL Equip24Kind::DeleteRecord(RPTR index)
{
	if(CreationError) return -1;
	DFileMan.DeleteRecord(FilePoint,index);
	return TRUE;
}

BOOL Equip24Kind::GetRecord(KIND24& buf,RPTR index)
{
	if(CreationError) return FALSE;
	if(!DFileMan.IsUseFul(FilePoint,index)) return FALSE;
	if(DFileMan.GetRecord(FilePoint,index,(char *)&buf,DFileMan.FileHeader[FilePoint].Record_Length)) return TRUE;
	return FALSE;
}

RPTR Equip24Kind::PutRecord(KIND24& buf)
{
	if(CreationError) return -1;
	return DFileMan.NewRecord(FilePoint,(char *)&buf);
}

RPTR Equip24Kind::RecordingPosition()
{
	if(CreationError) return -1;
	return DFileMan.GetRecordingPosition(FilePoint);
}

void Equip24Kind::InitFile(char *fname)
{
	if(fname) {
		int16 len=strlen(fname);
		if(len>12) len=12;
		strncpy(FileName,fname,len);
		FileName[12]=0;
	}
	else strcpy(FileName,EQP24KIND);
}

BOOL Equip24Kind::Compare(KIND24& name1,char *buf,WORD compkind)
{
	BOOL rtv=FALSE;
	if(compkind==0x0000) return TRUE;
	if(compkind==0x0001&&!strcmp(name1.Name,buf)) rtv=TRUE;
	if(compkind==0x0002&&!strcmp(name1.Number,buf)) rtv=TRUE;
	if(compkind==0x0004&&!strcmp(name1.Date,buf)) rtv=TRUE;
	if(compkind==0x0008&&name1.Inga>0) rtv=TRUE;
	if(compkind==0x0010&&name1.A>0) rtv=TRUE;
	if(compkind==0x0020&&name1.B>0) rtv=TRUE;
	if(compkind==0x0040&&name1.C>0) rtv=TRUE;

	return rtv;
}

BOOL Equip24Kind::Compare(KIND24& name1,KIND24& name2,WORD compkind=0x0007)
{
	BOOL rtv=FALSE;
	if(compkind==0x0000) return TRUE;
	if(compkind&0x0001){
		if(!strcmp(name1.Name,name2.Name)) rtv=TRUE;
		else return FALSE;
	}
	if(compkind&0x0002){
		if(!strcmp(name1.Number,name2.Number)) rtv=TRUE;
		else return FALSE;
	}
	if(compkind&0x0004){
		if(!strcmp(name1.Date,name2.Date)) rtv=TRUE;
		else return FALSE;
	}
	if(compkind&0x0008){
		if(name1.Inga>0) rtv=TRUE;
		else return FALSE;
	}
	if(compkind&0x0010){
		if(name1.A>0) rtv=TRUE;
		else return FALSE;
	}
	if(compkind&0x0020){
		if(name1.B>0) rtv=TRUE;
		else return FALSE;
	}
	if(compkind&0x0040){
		if(name1.C>0) rtv=TRUE;
		else return FALSE;
	}

	return rtv;
}

RPTR Equip24Kind::Search(char *buf,RPTR index=0,WORD compkind=0x0001)
{
	while(index<DFileMan.FileHeader[FilePoint].Record_Count) {
		if(GetRecord(WorkRecord,index)){
			if(Compare(WorkRecord,buf,compkind)) return index;
		}
		index++;
	}
	return -1;
}

RPTR Equip24Kind::Search(KIND24& name2,RPTR index=0,WORD compkind=0x00013)
{
	while(index<DFileMan.FileHeader[FilePoint].Record_Count) {
		if(GetRecord(WorkRecord,index)){
			if(Compare(WorkRecord,name2,compkind)) return index;
		}
		index++;
	}
	return -1;
}



