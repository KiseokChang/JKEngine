#include "tableman.h"

TableManager::TableManager(RecordBase*& rec,uint16& alloccount,uint16& realcount):
	RefRecord(rec),AllocCount(alloccount),RealCount(realcount),TableHead(-1),TableTail(-1)
{
	strcpy(TableFile,"noname.tbl");
	HeaderSize=sizeof(FILEHEADER)+sizeof(int16)*2;
}

TableManager::~TableManager()
{
}

BOOL TableManager::ChangeFileName(char *fname)
{
	FILEHEADER header;
	if(header.GetFileHeader(fname)){
		if(RefRecord->RecordID==header.RecordID&&
					AllocCount==header.AllocCount&&RealCount==header.RealCount){
			strncpy(TableFile,fname,MAXPATH);
			TableFile[MAXPATH]=0;
			FILE *fp = fopen(TableFile,"rb");
			fseek(fp,sizeof(header),SEEK_SET);
			fread(&TableHead,sizeof(TableHead),1,fp);
			fread(&TableTail,sizeof(TableTail),1,fp);
			fclose(fp);
			return TRUE;
		}
	}
	return FALSE;
}

void TableManager::PutHeader(FILE *fp)
{
	BOOL isopen=FALSE;
	if(!fp) {
		isopen=TRUE;
		fp = fopen(TableFile,"rb+");
	}
	if(fp){
		fseek(fp,0L,SEEK_SET);
		fwrite(&RefRecord->RecordID,sizeof(RefRecord->RecordID),1,fp);
		fwrite(&AllocCount,sizeof(AllocCount),1,fp);
		fwrite(&RealCount,sizeof(RealCount),1,fp);
		fwrite(&TableHead,sizeof(TableHead),1,fp);
		fwrite(&TableTail,sizeof(TableTail),1,fp);
		if(isopen) fclose(fp);
	}
}

uint16 TableManager::ExpandSpace(uint16 count)
{
	if(count){
		FILE *fp = fopen(TableFile,"rb+");
		if(fp){
			fseek(fp,0l,SEEK_END);
			REFENTRY refentry={FALSE,-1,-1};
			uint16 size=count;
			while(size--) fwrite(&refentry,sizeof(refentry),1,fp);
			PutHeader(fp);
			fclose(fp);
			return count;
		}
	}
	return 0;
}

BOOL TableManager::MakeEmptyFile()
{
	TableHead=-1;  TableTail=-1;

	FILE *fp = fopen(TableFile,"wb");
	if(fp){
		PutHeader(fp);
		REFENTRY refentry={FALSE,-1,-1};
		BYTE temp;
		for(uint16 i=0;i<AllocCount;i++)
			fwrite(&refentry,sizeof(refentry),1,fp);
		fclose(fp);
		return TRUE;
	}
	return FALSE;
}

int16 TableManager::FindEmptyIndex()
{
	REFENTRY refentry;
	FILE *TblFp = fopen(TableFile,"rb+");
	if(TblFp){
		fseek(TblFp,(int32)HeaderSize,SEEK_SET);
		uint16 index=0;
		while(index<AllocCount){
			fread(&refentry,sizeof(refentry),1,TblFp);
			if(!refentry.HasData) return index;
			index++;
		}
		fclose(TblFp);
	}
	return -1;
}

BOOL TableManager::GetRefEntry(uint16 index,REFENTRY& refentry)
{
	if(index<AllocCount){
		FILE *TblFp = fopen(TableFile,"rb");
		if(TblFp){
			fseek(TblFp,(int32)HeaderSize,SEEK_SET);
			fseek(TblFp,(int32)index*sizeof(refentry),SEEK_CUR);
			fread(&refentry,sizeof(refentry),1,TblFp);
			fclose(TblFp);
			return TRUE;
		}
	}
	return FALSE;
}

BOOL TableManager::PutRefEntry(uint16 index,REFENTRY& refentry)
{
	if(index<AllocCount){
		FILE *TblFp = fopen(TableFile,"rb+");
		if(TblFp){
			fseek(TblFp,(int32)HeaderSize,SEEK_SET);
			fseek(TblFp,(int32)index*sizeof(refentry),SEEK_CUR);
			fwrite(&refentry,sizeof(refentry),1,TblFp);
			fclose(TblFp);
			return TRUE;
		}
	}
	return FALSE;
}

int16 TableManager::GetPrevIndex(uint16 index)
{
	REFENTRY refentry;
	if(GetRefEntry(index,refentry))
		return refentry.prev;
	return -1;
}

int16 TableManager::GetNextIndex(uint16 index)
{
	REFENTRY refentry;
	if(GetRefEntry(index,refentry))
		return refentry.next;
	return -1;
}

void TableManager::AddRefEntry(uint16 allocindex)
{
	REFENTRY refentry={TRUE,-1,-1},temp;
	if(TableTail!=-1){
		GetRefEntry(TableTail,temp);
		temp.next=allocindex;
		PutRefEntry(TableTail,temp);
		refentry.prev=TableTail;
		TableTail=allocindex;
	}
	else{
		TableHead=allocindex;
		TableTail=allocindex;
	}
	PutRefEntry(allocindex,refentry);
}

void TableManager::InsertRefEntry(int16 index,uint16 allocindex)
{
	REFENTRY refentry={TRUE,-1,-1},temp;
	if(index!=-1&&TableHead!=-1){
		GetRefEntry(index,temp);
		if(temp.HasData){
			refentry.next=index;
			refentry.prev=temp.prev;
			temp.prev=allocindex;
			PutRefEntry(index,temp);
			if(refentry.prev==-1){
				TableHead=allocindex;
			}
			else {
				GetRefEntry(refentry.prev,temp);
				temp.next=allocindex;
				PutRefEntry(refentry.prev,temp);
			}
			PutRefEntry(allocindex,refentry);
			return;
		}
	}
	AddRefEntry(allocindex);
}

BOOL TableManager::FreeIndex(uint16 index)
{
	if(index<AllocCount){
		REFENTRY refentry;
		if(GetRefEntry(index,refentry)&&refentry.HasData){
			REFENTRY temp;
			if(refentry.prev!=-1){
				if(GetRefEntry(refentry.prev,temp)){
					temp.next=refentry.next;
					PutRefEntry(refentry.prev,temp);
				}
			}
			else TableHead=refentry.next;
			if(refentry.next!=-1){
				if(GetRefEntry(refentry.next,temp)){
					temp.prev=refentry.prev;
					PutRefEntry(refentry.next,temp);
				}
			}
			else TableTail=refentry.prev;
			refentry.prev=refentry.next=-1;
			refentry.HasData=FALSE;
			PutRefEntry(index,refentry);
			return TRUE;
		}
	}
	return FALSE;
}

BOOL TableManager::ReArrangeTable(uint16 *recarray)
{
	if(!RealCount||!recarray) return FALSE;

	FILE *fp = fopen(TableFile,"wb");
	if(!fp) return FALSE;

	REFENTRY refentry={FALSE,-1,-1};
	uint16 i;

	TableHead=recarray[0];
	TableTail=recarray[RealCount-1];
	PutHeader(fp);
	for(i=0;i<AllocCount;i++)
		fwrite(&refentry,sizeof(refentry),1,fp);
	fclose(fp);

	refentry.HasData=TRUE;
	for(i=0;i<RealCount;i++){
		refentry.prev=(i==0)? -1:recarray[i-1];
		refentry.next=(i==RealCount-1)? -1:recarray[i+1];
		PutRefEntry(recarray[i],refentry);
	}
	return TRUE;
}


