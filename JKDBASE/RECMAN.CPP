#include "recman.h"

RecordManager::RecordManager(RecordBase*& rec,uint16& alloccount,uint16& realcount,uint16& recsize):
	RefRecord(rec),AllocCount(alloccount),RealCount(realcount),RecordSize(recsize)
{
	strcpy(DataFile,"noname.rec");
	HeaderSize=sizeof(FILEHEADER);
}

RecordManager::~RecordManager()
{}

BOOL RecordManager::ChangeFileName(char *fname)
{
	FILEHEADER header;
	if(header.GetFileHeader(fname)){
		if(RefRecord->RecordID==header.RecordID&&
					AllocCount==header.AllocCount&&RealCount==header.RealCount){
			strncpy(DataFile,fname,MAXPATH);
			DataFile[MAXPATH]=0;
			return TRUE;
		}
	}
	return FALSE;
}

void RecordManager::PutHeader(FILE *fp)
{
	BOOL isopen=FALSE;
	if(!fp) {
		isopen=TRUE;
		fp = fopen(DataFile,"rb+");
	}
	if(fp){
		fseek(fp,0L,SEEK_SET);
		fwrite(&RefRecord->RecordID,sizeof(RefRecord->RecordID),1,fp);
		fwrite(&AllocCount,sizeof(AllocCount),1,fp);
		fwrite(&RealCount,sizeof(RealCount),1,fp);
		if(isopen) fclose(fp);
	}
}

uint16 RecordManager::ExpandSpace(uint16 count)
{
	if(count){
		FILE *fp = fopen(DataFile,"rb+");
		if(fp){
			fseek(fp,0l,SEEK_END);
			uint32 size=RecordSize*count;
			while(size--) fputc(0,fp);
			PutHeader(fp);
			fclose(fp);
			return count;
		}
	}
	return 0;
}

BOOL RecordManager::MakeEmptyFile()
{
	FILE *fp = fopen(DataFile,"wb");
	if(fp){
		PutHeader(fp);
		BYTE temp=' ';
		for(uint16 i=0;i<AllocCount;i++)
			fwrite(&temp,1,RecordSize,fp);
		fclose(fp);
		return TRUE;
	}
	return FALSE;
}

BOOL RecordManager::GetRecord(uint16 index,RecordBase& rec)
{
	if(index<AllocCount){
		FILE *fp=fopen(DataFile,"rb");
		if(fp){
			fseek(fp,(int32)HeaderSize,SEEK_SET);
			fseek(fp,(int32)index*RecordSize,SEEK_CUR);
			rec.Get(fp);
			fclose(fp);
			return TRUE;
		}
	}
	return FALSE;
}

BOOL RecordManager::PutRecord(uint16 index,RecordBase& rec)
{
	if(index<AllocCount){
		FILE *fp=fopen(DataFile,"rb+");
		if(fp){
			fseek(fp,(int32)HeaderSize,SEEK_SET);
			fseek(fp,(int32)index*RecordSize,SEEK_CUR);
			rec.Put(fp);
			fclose(fp);
			return TRUE;
		}
	}
	return FALSE;
}


