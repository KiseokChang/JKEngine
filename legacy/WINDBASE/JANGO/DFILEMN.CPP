#include "dfilemn.h"
#include <mem.h>

DataFileManager::DataFileManager()
{
	InitHandle();
}
DataFileManager::~DataFileManager()
{
	FileAllClose();
}

void DataFileManager::InitHandle()
{
	for(int16 i=0;i<MAXFILS;i++) Handle[i]=0;
}

int16 DataFileManager::ChangeFileName(char *name,int16 len)
{
	struct ffblk ffblk;

	FileAllClose();
	if(findfirst(name,&ffblk,FA_ARCH)) FileCreate(name,len);
	return FileOpen(name);
}

void DataFileManager::FileCreate(char *name,int16 len)
{
	int16 fp;
	FHEADER hd;
	unlink(name);
	fp=creat(name,CREATMODE);
	close(fp);
	fp=open(name,OPENMODE);
	hd.Record_Count=1;
	hd.Delete_Record=0;
	hd.Record_Length=len;
	write(fp,(char *)&hd,sizeof(hd));
	close(fp);
}

int16 DataFileManager::FileOpen(char *name)
{
	int16 fp;
	for(fp=0;fp<MAXFILS;fp++) if(Handle[fp]==0) break;
	if(fp>=MAXFILS) return -1;
	if((Handle[fp]=open(name,OPENMODE))==-1) return -1;
	GetHead(fp);
	return fp;
}

void DataFileManager::FileClose(int16 fp)
{
	lseek(Handle[fp],0L,0);
	write(Handle[fp],(char *)&FileHeader[fp],sizeof(FHEADER));
	close(Handle[fp]);
	Handle[fp]=0;
}

int16 DataFileManager::GetHead(int16 fp)
{
	if(fp>=MAXFILS) return FALSE;
	lseek(Handle[fp],0L,0);
	read(Handle[fp],(char *)&FileHeader[fp],sizeof(FHEADER));
	return TRUE;
}

int16 DataFileManager::PutHead(int16 fp)
{
	if(fp>=MAXFILS) return FALSE;
	lseek(Handle[fp],0L,0);
	write(Handle[fp],(char *)&FileHeader[fp],sizeof(FHEADER));
	return TRUE;
}

RPTR DataFileManager::NewRecord(int16 fp,char *bf)
{
	RPTR rcdno;
	FHEADER fhead;
	if(FileHeader[fp].Delete_Record) {
		rcdno=FileHeader[fp].Delete_Record;
		if(GetRecord(fp,rcdno,(char *)&fhead,sizeof(FHEADER))==FALSE) return -1;
		FileHeader[fp].Delete_Record=fhead.Record_Count;
	}
	else rcdno=FileHeader[fp].Record_Count++;
	if((PutRecord(fp,rcdno,bf,FileHeader[fp].Record_Length))==FALSE) return -1;
	SetUseFul(fp,rcdno,TRUE);
	if(!PutHead(fp)) return -1;
	return (RPTR)rcdno;
}

int16 DataFileManager::GetRecord(int16 fp,RPTR rcdno,char *bf,int16 length)
{
	if(rcdno>=FileHeader[fp].Record_Count) return FALSE;
	if(FileHeader[fp].Record_Length<length) return FALSE;
	lseek(Handle[fp],FLOCATE(rcdno,FileHeader[fp].Record_Length),0);
	read(Handle[fp],bf,length);
	return TRUE;
}

int16 DataFileManager::PutRecord(int16 fp,RPTR rcdno,char *bf,int16 length)
{
	if(rcdno>FileHeader[fp].Record_Count) return FALSE;
	if(FileHeader[fp].Record_Length<length) return FALSE;
	lseek(Handle[fp],FLOCATE(rcdno,FileHeader[fp].Record_Length),0);
	write(Handle[fp],bf,length);
	return TRUE;
}

int16 DataFileManager::DeleteRecord(int16 fp,RPTR rcdno)
{
	FHEADER fhead;

	if(rcdno>FileHeader[fp].Record_Count) return FALSE;
	fhead.Record_Count=FileHeader[fp].Delete_Record;
	fhead.Delete_Record=rcdno;
	fhead.Record_Length=0;
	FileHeader[fp].Delete_Record=rcdno;
	SetUseFul(fp,rcdno,FALSE);
	PutHead(fp);
	PutRecord(fp,rcdno,(char *)&fhead,sizeof(fhead));
	return TRUE;
}

BOOL DataFileManager::SetUseFul(int16 fp,RPTR rcdno,BOOL isuseful)
{
	if(rcdno>FileHeader[fp].Record_Count) return FALSE;
	lseek(Handle[fp],USEFULLOCATE(rcdno,FileHeader[fp].Record_Length),0);
	write(Handle[fp],(char *)&isuseful,1);
	lseek(Handle[fp],0L,0);
	write(Handle[fp],(char *)&FileHeader[fp],sizeof(FHEADER));
	return TRUE;
}

BOOL DataFileManager::IsUseFul(int16 fp,RPTR rcdno)
{
	BOOL IsUseFul=FALSE;

	if(rcdno>FileHeader[fp].Record_Count) return FALSE;
	lseek(Handle[fp],USEFULLOCATE(rcdno,FileHeader[fp].Record_Length),0);
	read(Handle[fp],(char *)&IsUseFul,1);
	return IsUseFul;
}

void DataFileManager::FileAllClose()
{
	for(int16 i=0;i<MAXFILS;i++)
		if(Handle[i]) FileClose(i);
}

RPTR DataFileManager::GetRecordingPosition(int16 fp)
{
	RPTR rcdno;

	if(FileHeader[fp].Delete_Record) rcdno=FileHeader[fp].Delete_Record;
	else rcdno=FileHeader[fp].Record_Count;
	return (RPTR)rcdno;
}
