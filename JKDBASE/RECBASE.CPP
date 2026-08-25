#include "recbase.h"

RecordBase::RecordBase(WORD recid,char *name):
	DLLFrame<EntryBase>(),
	RecordID(recid)
{
	if(name) strncpy(RecordName,name,MAXNAME);
	else 		 strncpy(RecordName,"RecordBase",MAXNAME);
	RecordName[MAXNAME]='\0';
}

RecordBase::~RecordBase()
{}

//////////////////////////////////////////////////
RecordBase *RecordBase::CreateEqualRecord(RecordBase *rec,BOOL isdel)
{
	if(rec) {
		if(RecordID!=rec->RecordID) {
			if(isdel) delete rec;
			return 0;
		}
		rec->Assign(this);
	}
	return rec;
}

EntryBase *RecordBase::GetEntry(WORD entid)
{
	EntryBase *ent = HeadNode;
	while(ent) {
		if(ent->EntryID==entid)	return ent;
		ent=ent->next;
	}
	return 0;
}

EntryBase *RecordBase::GetEntry(EntryBase *ent)
{
	return (ent)?	GetEntry(ent->EntryID): 0;
}

BOOL RecordBase::AddEntry(EntryBase *ent)
{
	if(ent){
		ent->InitData();
		return PutNodeLast(ent);
	}
	return FALSE;
}

void RecordBase::DestroyEntry(WORD entid)
{
	EntryBase *ent = GetEntry(entid);
	if(ent) {
		if(ent->prev)	ent->prev->next=ent->next;
		else HeadNode=ent->next;
		if(ent->next)	ent->next->prev=ent->prev;
		else TailNode=ent->prev;
		delete ent;
		NodeCount--;
	}
}

void RecordBase::DestroyEntry(EntryBase *ent)
{
	if(ent) DestroyEntry(ent->EntryID);
}

void RecordBase::DestroyAllEntry()
{
	ClearNode();
}

BOOL RecordBase::IsEmpty()
{
	EntryBase *ent = TailNode;
	while(ent) {
		if(!ent->IsEmpty()) return FALSE;
		ent=ent->prev;
	}
	return TRUE;
}

BOOL RecordBase::IsFullfilled()
{
	EntryBase *ent = TailNode;
	while(ent) {
		if(ent->IsEmpty()) return FALSE;
		ent=ent->prev;
	}
	return TRUE;
}

uint16 RecordBase::GetMaxBufferSize()
{
	uint16 total=0,temp;

	EntryBase *ent = TailNode;
	while(ent) {
		temp = ent->BufferSize;
		if(total<temp) total=temp;
		ent=ent->prev;
	}
	return total;
}

uint16 RecordBase::GetFileSize()
{
	uint16 total=0;
	EntryBase *ent = HeadNode;
	while(ent) {
		total+=ent->FileSize;
		ent=ent->next;
	}
	return total;
}

BOOL RecordBase::SetData(WORD entid,BYTE *data)
{
	EntryBase *ent = GetEntry(entid);
	if(ent) return ent->SetData(data);
	return FALSE;
}

BOOL RecordBase::GetData(WORD entid,BYTE *data)
{
	EntryBase *ent = GetEntry(entid);
	if(ent) return ent->GetData(data);
	return FALSE;
}


int16  RecordBase::Compare(EntryBase *ent,uint16 compkind)
{
	EntryBase *ptrent=GetEntry(ent);
	if(ptrent) return ptrent->Compare(ent,compkind);
	return 2;
}

int16  RecordBase::Compare(RecordBase *rec,WORD entid,uint16 compkind)
{
	if(rec&&RecordID==rec->RecordID){
		return Compare(rec->GetEntry(entid),compkind);
	}
	return 2;
}

int16 RecordBase::Compare(RecordBase *rec,WORD *comporder,uint16 ordercount,uint16 *compkind)
{
	if(rec&&RecordID==rec->RecordID && comporder&&ordercount){
		int16 result;
		EntryBase *ent;
		for(uint16 i=0;i<ordercount;i++){
			result=Compare(rec,comporder[i],compkind? compkind[i]:0);
			if(result) return result;
		}
		return 0;
	}
	return 2;
}

BOOL RecordBase::CompareScope(RecordBase *min,RecordBase *max,uint16 *compoder,uint16 ordercount,uint16 *compkind)
{
	int16 comp1=Compare(min,compoder,ordercount,compkind);
	int16 comp2=Compare(max,compoder,ordercount,compkind);
	if(comp1>=0&&comp1!=2&&comp2<=0) return TRUE;
	return FALSE;
}

BOOL RecordBase::CompareScope(RecordBase *min,RecordBase *max,uint16 index,uint16 compkind)
{
	int16 comp1=Compare(min,index,compkind);
	int16 comp2=Compare(max,index,compkind);
	if(comp1>=0&&comp1!=2&&comp2<=0) return TRUE;
	return FALSE;
}

BOOL RecordBase::CompareScope(EntryBase *min,EntryBase *max,uint16 compkind)
{
	int16 comp1=Compare(min,compkind);
	int16 comp2=Compare(max,compkind);
	if(comp1>=0&&comp1!=2&&comp2<=0) return TRUE;
	return FALSE;
}

//////////////////////////////////////////////////

void RecordBase::InitData()
{
	EntryBase *ent = HeadNode;
	while(ent) {
		ent->InitData();
		ent=ent->next;
	}
}

BOOL RecordBase::Assign(EntryBase *ent)
{
	EntryBase *ptrent = GetEntry(ent);
	if(ptrent)	return ptrent->Assign(ent);
	return FALSE;
}

BOOL RecordBase::Assign(RecordBase *rec)
{
	if(rec&&RecordID==rec->RecordID) {
		EntryBase *ent1 = HeadNode;
		EntryBase *ent2 = rec->HeadNode;
		while(ent1&&ent2) {
			if(!ent1->Assign(ent2)) return FALSE;
			ent1=ent1->next;
			ent2=ent2->next;
		}
		return TRUE;
	}
	return FALSE;
}

void RecordBase::Get(FILE *fp)
{
	EntryBase *ent = HeadNode;
	while(ent) {
		ent->Get(fp);
		ent=ent->next;
	}
}

void RecordBase::Put(FILE *fp)
{
	EntryBase *ent = HeadNode;
	while(ent) {
		ent->Put(fp);
		ent=ent->next;
	}
}
//////////////////////////////////////////////////
BOOL RecordBase::IsEqual(RecordBase *rec,uint16 compkind)
{
	if(rec&&RecordID==rec->RecordID) {
		if(compkind&0x0001) {
			EntryBase *ent1 = HeadNode,*ent2 = rec->HeadNode;
			while(ent1&&ent2) {
				if(ent1->IsEqual(ent2,0x0001)) return FALSE;
				ent1=ent1->next; ent2=ent2->next;
			}
			return (!ent1&&!ent2)? TRUE:FALSE;
		}
		else return TRUE;
	}
	return FALSE;
}


