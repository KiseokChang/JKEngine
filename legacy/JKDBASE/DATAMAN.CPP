#include "dataman.h"

DataManager::DataManager():
//	prev(0),next(0),
	WorkRecord(0),AllocCount(0),RealCount(0),RecordSize(0),
	RecordMan(WorkRecord,AllocCount,RealCount,RecordSize),
	TableMan(WorkRecord,AllocCount,RealCount),
	CreationError(FALSE)
{}

DataManager::~DataManager()
{
	if(WorkRecord) delete WorkRecord;
}

////////////////////////////////////////////////////////////////////////////////

BOOL DataManager::ChangeFileName(char *fname,RecordBase& rec)
{
	if(fname&&fname[0]){
		RecordBase *TempRecord = rec.Duplicate();
		if(TempRecord){
			if(WorkRecord) delete WorkRecord;
			WorkRecord = TempRecord;

			RecordSize=WorkRecord->GetFileSize();

			char mainfname[8+1];
			strncpy(mainfname,fname,8);
			mainfname[8]=0;

			char recfname[13+1],tblfname[13+1];
			strcpy(recfname,mainfname);
			strcat(recfname,".dat");
			strcpy(tblfname,mainfname);
			strcat(tblfname,".tbl");

			FILEHEADER recheader,tblheader;
			if(recheader.GetFileHeader(recfname)){
				AllocCount=recheader.AllocCount;
				RealCount=recheader.RealCount;
				if(RecordMan.ChangeFileName(recfname)&&TableMan.ChangeFileName(tblfname))
					return TRUE;
			}
			strcpy(RecordMan.DataFile,recfname);
			strcpy(TableMan.TableFile,tblfname);
			MakeEmptyFile(0);
			return TRUE;
		}
	}
	return FALSE;
}

////////////////////////////////////////////////////////////////////////////

int16 DataManager::AllocIndex()
{
	int16 index=TableMan.FindEmptyIndex();
	if(index!=-1) return index;

	if(TableMan.ExpandSpace(1)&&	RecordMan.ExpandSpace(1)){
		AllocCount++;
		return AllocCount-1;
	}
	return -1;
}

BOOL DataManager::MakeEmptyFile(uint16 alloccount)
{
	AllocCount=alloccount; 	RealCount=0;
	if(TableMan.MakeEmptyFile() &&	RecordMan.MakeEmptyFile()){
		return TRUE;
	}
	return FALSE;
}

BOOL DataManager::DeleteRecord(uint16 index)
{
	if(TableMan.FreeIndex(index)){
		RealCount--;
		TableMan.PutHeader();
		RecordMan.PutHeader();
		return TRUE;
	}
  return FALSE;
}

BOOL DataManager::DeleteAll()
{
	REFENTRY refentry;
	int16 index=TableMan.TableHead;
	while(index!=-1&&TableMan.GetRefEntry(index,refentry)){
		if(refentry.HasData) {
			int16 next=refentry.next;
			DeleteRecord(index);
			index=next;
		}
		else index=-1;
	}
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////////

int16 DataManager::AddRecord(RecordBase& rec)
{
	if(!WorkRecord->IsEqual(&rec)) return FALSE;
	int16 allocindex=AllocIndex();
	if(allocindex!=-1){
		RecordMan.PutRecord(allocindex,rec);
		TableMan.AddRefEntry(allocindex);
		RealCount++;

		RecordMan.PutHeader();
		TableMan.PutHeader();
		return allocindex;
	}
	return -1;
}

int16 DataManager::InsertRecord(uint16 index,RecordBase& rec)
{
	if(!WorkRecord->IsEqual(&rec)) return -1;
	int16 allocindex=AllocIndex();
	if(allocindex!=-1){
		RecordMan.PutRecord(allocindex,rec);
		TableMan.InsertRefEntry(index,allocindex);
		RealCount++;

		RecordMan.PutHeader();
		TableMan.PutHeader();
		return allocindex;
	}
	return -1;
}

BOOL DataManager::SetRecord(uint16 index,RecordBase& rec)
{
	REFENTRY refentry;
	if(TableMan.GetRefEntry(index,refentry)&&refentry.HasData)
		return RecordMan.PutRecord(index,rec);
	return FALSE;
}

/************************************************************************/

int16 DataManager::Search(EntryBase *ent,int16 findstart)
{
	if(!ent) return -1;

	REFENTRY refentry;
	int16 index=-1;

	if(findstart<0) {
		index=TableMan.TableHead;
	}
	else{
		if(TableMan.GetRefEntry(findstart,refentry)){
			if(refentry.HasData) index=refentry.next;
		}
	}

	while(index!=-1&&TableMan.GetRefEntry(index,refentry)){
		if(refentry.HasData) {
			RecordMan.GetRecord(index,*WorkRecord);
			if(!WorkRecord->Compare(ent))	return index;
			index=refentry.next;
		}
		else index=-1;
	}
	return -1;
}
int16 DataManager::SearchScope(EntryBase *min,EntryBase *max,int16 findstart)
{
	if(!min||!max) return -1;

	REFENTRY refentry;
	int16 index=-1;

	if(findstart<0) {
		index=TableMan.TableHead;
	}
	else{
		if(TableMan.GetRefEntry(findstart,refentry)){
			if(refentry.HasData) index=refentry.next;
			else index=-1;
		}
	}
	while(index!=-1&&TableMan.GetRefEntry(index,refentry)){
		if(refentry.HasData) {
			RecordMan.GetRecord(index,*WorkRecord);
			if(WorkRecord->CompareScope(min,max))	return index;
			index=refentry.next;
		}
		else index=-1;
	}
	return -1;
}

int16 DataManager::Search(RecordBase *rec,WORD entid,int16 findstart)
{
	if(!WorkRecord->IsEqual(rec))	return -1;
	return Search(rec->GetEntry(entid),findstart);
}

int16 DataManager::SearchScope(RecordBase *min,RecordBase *max,WORD entid,int16 findstart)
{
	if(!WorkRecord->IsEqual(min)||!WorkRecord->IsEqual(max)) return -1;
	return SearchScope(min->GetEntry(entid),max->GetEntry(entid),findstart);
}

int16 DataManager::Search(RecordBase *rec,WORD *compoder,uint16 ordercount,int16 findstart)
{
	if(!WorkRecord->IsEqual(rec))	return -1;
	if(!compoder||!ordercount) return -1;

	REFENTRY refentry;
	int16 index=-1;

	if(findstart<0) {
		index=TableMan.TableHead;
	}
	else{
		if(TableMan.GetRefEntry(findstart,refentry)){
			if(refentry.HasData) index=refentry.next;
			else index=-1;
		}
	}

	while(index!=-1&&TableMan.GetRefEntry(index,refentry)){
		if(refentry.HasData) {
			RecordMan.GetRecord(index,*WorkRecord);
			if(!WorkRecord->Compare(rec,compoder,ordercount))	return index;
			index=refentry.next;
		}
		else index=-1;
	}
	return -1;
}


int16 DataManager::SearchScope(RecordBase *min,RecordBase *max,WORD *compoder,uint16 ordercount,int16 findstart)
{
	if(!WorkRecord->IsEqual(min)||!WorkRecord->IsEqual(max)) return -1;
	if(!compoder||!ordercount) return -1;

	REFENTRY refentry;
	int16 index=-1;

	if(findstart<0) {
		index=TableMan.TableHead;
	}
	else{
		if(TableMan.GetRefEntry(findstart,refentry)){
			if(refentry.HasData) index=refentry.next;
			else index=-1;
		}
	}

	while(index!=-1&&TableMan.GetRefEntry(index,refentry)){
		if(refentry.HasData) {
			RecordMan.GetRecord(index,*WorkRecord);
			if(WorkRecord->CompareScope(min,max,compoder,ordercount))	return index;
			index=refentry.next;
		}
		else index=-1;
	}
	return -1;
}

BOOL DataManager::Sort(uint16 *recarray,uint16 reccount,uint16 entid)
{
	if(!recarray||reccount<2) return FALSE;
	RecordBase *WorkRecord2=WorkRecord->Duplicate();
	if(!WorkRecord2) return FALSE;

	RecordBase *TempRecord[2]={WorkRecord,WorkRecord2};
	BOOL flag=FALSE;

	BOOL IsChange=FALSE;
	for(uint16 i=0;i<reccount-1;i++) {
		flag=FALSE;
		RecordMan.GetRecord(recarray[i],*TempRecord[0]);
		for(uint16 j=i+1;j<reccount;j++) {
			RecordMan.GetRecord(recarray[j],*TempRecord[flag? 0:1]);
			if(TempRecord[flag? 1:0]->Compare(TempRecord[flag? 0:1],entid)>0){
				uint16 temp=recarray[i];
				recarray[i]=recarray[j];
				recarray[j]=temp;
				IsChange=TRUE;
				flag = flag? 0:1;
			}
		}
	}
	delete WorkRecord2;
	return IsChange? FALSE:TRUE;
}

BOOL DataManager::Sort(uint16 *recarray,uint16 reccount,WORD *compoder,uint16 ordercount)
{
	if(!recarray||reccount<2) return FALSE;
	if(!compoder||!ordercount) return FALSE;
	RecordBase *WorkRecord2=WorkRecord->Duplicate();
	if(!WorkRecord2) return FALSE;

	RecordBase *TempRecord[2]={WorkRecord,WorkRecord2};
	BOOL flag=FALSE;

	BOOL IsChange=FALSE;
	for(uint16 i=0;i<reccount-1;i++) {
		flag=FALSE;
		RecordMan.GetRecord(recarray[i],*TempRecord[0]);
		for(uint16 j=i+1;j<reccount;j++) {
			RecordMan.GetRecord(recarray[j],*TempRecord[flag? 0:1]);
			if(TempRecord[flag? 1:0]->Compare(TempRecord[flag? 0:1],compoder,ordercount)>0){
				uint16 temp=recarray[i];
				recarray[i]=recarray[j];
				recarray[j]=temp;
				IsChange=TRUE;
				flag = flag? 0:1;
			}
		}
	}
	delete WorkRecord2;
	return IsChange? FALSE:TRUE;
}

BOOL DataManager::FileSort(uint16 entid)
{
	if(RealCount<2) return FALSE;
	uint16 *recarray=new uint16[RealCount];
	if(!recarray) return FALSE;
	uint16  reccount=0;

	REFENTRY refentry;
	int16 index=TableMan.TableHead;
	while(index!=-1&&TableMan.GetRefEntry(index,refentry)){
		if(refentry.HasData) {
			recarray[reccount++]=index;
			index=refentry.next;
		}
		else index=-1;
	}
	if(reccount!=RealCount){
		delete recarray;
		return FALSE;
	}

	Sort(recarray,RealCount,entid);
	TableMan.ReArrangeTable(recarray);
	delete recarray;
	return TRUE;
}

BOOL DataManager::FileSort(WORD *compoder,uint16 ordercount)
{
	if(RealCount<2) return FALSE;
	uint16 *recarray=new uint16[RealCount];
	if(!recarray) return FALSE;
	uint16  reccount=0;

	REFENTRY refentry;
	int16 index=TableMan.TableHead;
	while(index!=-1&&TableMan.GetRefEntry(index,refentry)){
		if(refentry.HasData) {
			recarray[reccount++]=index;
			index=refentry.next;
		}
		else index=-1;
	}
	if(reccount!=RealCount){
		delete recarray;
		return FALSE;
	}

	Sort(recarray,RealCount,compoder,ordercount);
	TableMan.ReArrangeTable(recarray);
	delete recarray;
	return TRUE;
}

/*
BOOL DataManager:: IsEqual(DataManager& other,uint16 compkind)
{

}
*/
