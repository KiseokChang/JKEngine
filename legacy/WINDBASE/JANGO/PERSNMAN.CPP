#include "joglobal.h"
#include "persnman.h"

PersonManager::PersonManager(char *fname,char *obsvname):
	RecordManager(),AddedRecord(0),ObserveMan(obsvname),OfficerCount(0),ManagerMode(TRUE)
{
	TableHeaderSize+=sizeof(uint16);
	DataHeaderSize+=sizeof(uint16);

	AddedRecord=new PersonRecord();
	if(AddedRecord) ChangeRecordBase(fname,*AddedRecord);
}

PersonManager::~PersonManager()
{
	if(AddedRecord) delete AddedRecord;
}


BOOL PersonManager::ChangeRecordBase(char *fname,RecordBase& rec)
{
	char tempbuf[8+1];
	if(fname){
		strncpy(tempbuf,fname,8);
		tempbuf[8]=0;
	}
	else {
		strcpy(tempbuf,BudaeName);
		strcat(tempbuf,"main");
	}
	ManagerMode=TRUE;
	return RecordManager::ChangeRecordBase(tempbuf,rec);
}

void PersonManager::ChangeManagerMode(BOOL mode)
{
	if(ManagerMode!=mode){
		strcpy(DataFile,RecFileName);
		strcat(DataFile,mode? ".dat":".odt");
		strcpy(TableFile,RecFileName);
		strcat(TableFile,mode? ".tbl":".otb");
		InitByFile();
		ManagerMode=mode;
	}
}


void PersonManager::InitByFile()
{
	FILE *TblFp = fopen(TableFile,"rb");
	FILE *DataFp= fopen(DataFile,"rb");
	if(TblFp&&DataFp){
		uint16 RecID1,RecID2;
		uint16 AllocCnt1,AllocCnt2;
		uint16 RealCnt1,RealCnt2;
		uint16 OfficerCnt1,OfficerCnt2;
		int16 header,tail;
		fread(&RecID1,sizeof(RecID1),1,TblFp);
		fread(&AllocCnt1,sizeof(AllocCount),1,TblFp);
		fread(&RealCnt1,sizeof(RealCount),1,TblFp);
		fread(&OfficerCnt1,sizeof(OfficerCount),1,TblFp);
		fread(&header,sizeof(header),1,TblFp);
		fread(&tail,sizeof(tail),1,TblFp);
		fread(&RecID2,sizeof(RecID2),1,DataFp);
		fread(&AllocCnt2,sizeof(AllocCount),1,DataFp);
		fread(&RealCnt2,sizeof(RealCount),1,DataFp);
		fread(&OfficerCnt2,sizeof(OfficerCount),1,DataFp);
		if(WorkRecord->RecordID==RecID1&&RecID1==RecID2&&
						AllocCnt1==AllocCnt2&&RealCnt1==RealCnt2&&OfficerCnt1==OfficerCnt2){
			AllocCount=AllocCnt1;
			RealCount=RealCnt1;
			OfficerCount=OfficerCnt1;
			TableHead=header;
			TableTail=tail;
		}
		fclose(TblFp);
		fclose(DataFp);
		return;
	}
	if(TblFp) fclose(TblFp);
	if(DataFp) fclose(DataFp);
	MakeEmptyFile(0);
}

BOOL PersonManager::MakeEmptyFile(uint16 alloccount)
{
	OfficerCount=0;
	return RecordManager::MakeEmptyFile(alloccount);
}

void PersonManager::PutHeader(FILE *fp1,FILE *fp2)
{
	if(fp1){
		fseek(fp1,0L,SEEK_SET);
		fwrite(&WorkRecord->RecordID,sizeof(WorkRecord->RecordID),1,fp1);
		fwrite(&AllocCount,sizeof(AllocCount),1,fp1);
		fwrite(&RealCount,sizeof(RealCount),1,fp1);
		fwrite(&OfficerCount,sizeof(OfficerCount),1,fp1);
		fwrite(&TableHead,sizeof(TableHead),1,fp1);
		fwrite(&TableTail,sizeof(TableTail),1,fp1);
	}
	if(fp2){
		fseek(fp2,0L,SEEK_SET);
		fwrite(&WorkRecord->RecordID,sizeof(WorkRecord->RecordID),1,fp2);
		fwrite(&AllocCount,sizeof(AllocCount),1,fp2);
		fwrite(&RealCount,sizeof(RealCount),1,fp2);
		fwrite(&OfficerCount,sizeof(OfficerCount),1,fp2);
	}
}

BOOL PersonManager::FreeIndex(uint16 index)
{
	PersonRecord TempRecord;
	GetRecord(index,TempRecord);
	if(RecordManager::FreeIndex(index)){
		if(TempRecord.IsGanbu()) OfficerCount--;
		FILE *fp1 = fopen(TableFile,"rb+");
		FILE *fp2 = fopen(DataFile,"rb+");
		if(fp1&&fp2) PutHeader(fp1,fp2);
		if(fp1) fclose(fp1);
		if(fp2) fclose(fp2);

		if(ManagerMode)
			ObserveMan.FreeAll(TempRecord.ObserveIndex);

		return TRUE;
	}
	return FALSE;
}

BOOL PersonManager::BackupRecord(uint16 index)
{
	if(ManagerMode){
		PersonRecord TempRecord;
		GetRecord(index,TempRecord);
		if(RecordManager::FreeIndex(index)){
			if(TempRecord.IsGanbu()) OfficerCount--;
			FILE *fp1 = fopen(TableFile,"rb+");
			FILE *fp2 = fopen(DataFile,"rb+");
			if(fp1&&fp2) PutHeader(fp1,fp2);
			if(fp1) fclose(fp1);
			if(fp2) fclose(fp2);
			if(ManagerMode){
				ObserveMan.FreeAll(TempRecord.ObserveIndex);

				PersonManager::ChangeManagerMode(FALSE);
				TempRecord.ObserveIndex=-1;
				PersonManager::AddRecord(TempRecord);
				PersonManager::ChangeManagerMode(TRUE);
			}
			return TRUE;
		}
	}
	return FALSE;
}

BOOL PersonManager::SetRecord(uint16 index,RecordBase& rec)
{
	REFENTRY refentry;
	if(GetRefEntry(index,refentry)&&refentry.HasData){
		PersonRecord TempRecord;
		GetRecord(index,TempRecord);

		if(TempRecord.IsGanbu()){
			if(!((PersonRecord& )rec).IsGanbu()) OfficerCount--;
		}
		else{
			if(((PersonRecord& )rec).IsGanbu()) OfficerCount++;
		}
		FILE *fp1 = fopen(TableFile,"rb+");
		FILE *fp2 = fopen(DataFile,"rb+");
		if(fp1&&fp2) PutHeader(fp1,fp2);
		if(fp1) fclose(fp1);
		if(fp2) fclose(fp2);
		return PutRecord(index,rec);
	}
	return -1;
}

int16 PersonManager::AddRecord(RecordBase& rec)
{
	int16 allocindex=RecordManager::AddRecord(rec);
	if(allocindex!=-1){
		if(((PersonRecord& )rec).IsGanbu()) OfficerCount++;
		FILE *fp1 = fopen(TableFile,"rb+");
		FILE *fp2 = fopen(DataFile,"rb+");
		if(fp1&&fp2) PutHeader(fp1,fp2);
		if(fp1) fclose(fp1);
		if(fp2) fclose(fp2);
		return allocindex;
	}
	return -1;
}

int16 PersonManager::InsertRecord(uint16 index,RecordBase& rec)
{
	int16 allocindex=RecordManager::InsertRecord(index,rec);
	if(allocindex!=-1){
		if(((PersonRecord& )rec).IsGanbu()) OfficerCount++;
		FILE *fp1 = fopen(TableFile,"rb+");
		FILE *fp2 = fopen(DataFile,"rb+");
		if(fp1&&fp2) PutHeader(fp1,fp2);
		if(fp1) fclose(fp1);
		if(fp2) fclose(fp2);
		return allocindex;
	}
	return -1;
}


BOOL PersonManager::DeleteBudae(uint16 budae)
{
	if(budae<4){
		PersonRecord rec;
		REFENTRY refentry;
		int16	index=TableHead,next;
		while(index!=-1&&GetRefEntry(index,refentry)){
			if(refentry.HasData) {
				next=refentry.next;
				GetRecord(index,rec);
				if(rec.GetBudae()==budae)	FreeIndex(index);
				index=next;
			}
			else index=-1;
		}
		return TRUE;
	}
	return FALSE;
}

BOOL PersonManager::AddPersonMan(PersonManager *srcman,uint16 budae)
{
	if(srcman&&budae<4){
		DeleteBudae(budae);

		PersonRecord rec;

		REFENTRY refentry;
		int16	index=srcman->TableHead;
		while(index!=-1&&srcman->GetRefEntry(index,refentry)){
			if(refentry.HasData) {
				srcman->GetRecord(index,rec);
				if(rec.GetBudae()==budae)	{
					if(rec.ObserveIndex>=0){
						rec.ObserveIndex=ObserveMan.AddObserveMan(&srcman->ObserveMan,rec.ObserveIndex);
					}
					AddRecord(rec);
				}
				index=refentry.next;
			}
			else index=-1;
		}
		return TRUE;
	}
	return FALSE;
}
