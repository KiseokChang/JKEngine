#include "eq24dman.h"

Equip24DataManager::Equip24DataManager():Eqp24Kind(0),Eqp24Name(0)
{

}

Equip24DataManager::~Equip24DataManager()
{
}

RPTR Equip24DataManager::GetFirstIndex(char *s,WORD compkind=0x0001)
{
	RPTR index=Eqp24Name.Search(s,0,compkind);
	if(index==-1) return -1;
	if(Eqp24Name.GetRecord(index)) {
		return Eqp24Name.NameRecord.FirstIndex;
	}
	return -1;
}

RPTR Equip24DataManager::GetFirstIndex(NAME24& name,WORD compkind=0x0007)
{
	RPTR index=Eqp24Name.Search(name,0,compkind);
	if(index==-1) return -1;
	if(Eqp24Name.GetRecord(index)) {
		return Eqp24Name.NameRecord.FirstIndex;
	}
	return -1;
}

RPTR Equip24DataManager::GetNameIndex(KIND24& name,WORD compkind=0x003f)
{
	RPTR index=Eqp24Kind.Search(name,0,compkind);
	if(index==-1) return -1;
	if(Eqp24Kind.GetRecord(index)) {
		return Eqp24Kind.KindRecord.NameIndex;
	}
	return -1;
}


BOOL Equip24DataManager::PutRecord(EQP24& eqp24,WORD compname,WORD compkind)
{
	RPTR kindposition=0;
	RPTR nameposition=0;
	RPTR index,tempindex;

	index=GetFirstIndex(eqp24.Name24,compname);
	if(index==-1){
		nameposition=Eqp24Name.RecordingPosition();
		kindposition=Eqp24Kind.RecordingPosition();
		eqp24.Name24.FirstIndex=eqp24.Name24.LastIndex=kindposition;
		Eqp24Name.PutRecord(eqp24.Name24);
		eqp24.Kind24.NameIndex=nameposition;
		eqp24.Kind24.PrvIndex=eqp24.Kind24.NextIndex=-1L;
		return Eqp24Kind.PutRecord(eqp24.Kind24);
	}
	else{
		while(Eqp24Kind.GetRecord(index)&&index!=-1){
			if(Eqp24Kind.Compare(eqp24.Kind24,Eqp24Kind.KindRecord,compkind)){
				Eqp24Kind.KindRecord.A+=eqp24.Kind24.A;
				Eqp24Kind.KindRecord.B+=eqp24.Kind24.B;
				Eqp24Kind.KindRecord.C+=eqp24.Kind24.C;
				return Eqp24Kind.PutRecord(eqp24.Kind24,index);
			}
			tempindex=index;
			index=Eqp24Kind.KindRecord.NextIndex;
		}
		if(!Eqp24Kind.GetRecord(tempindex)) return FALSE;
		else {
			return AppendKind(eqp24.Kind24,tempindex);
		}
	}
}

BOOL Equip24DataManager::AppendKind(KIND24& kind,RPTR index)
{
	RPTR kindposition=Eqp24Kind.RecordingPosition();

	if(!Eqp24Kind.GetRecord(index)) return FALSE;
	kind.NameIndex=Eqp24Kind.KindRecord.NameIndex;
	kind.PrvIndex=index;
	kind.NextIndex=-1L;

	Eqp24Kind.KindRecord.NextIndex=kindposition;
	Eqp24Kind.PutRecord(Eqp24Kind.KindRecord,index);
	Eqp24Kind.PutRecord(kind);

	if(!Eqp24Name.GetRecord(Eqp24Kind.KindRecord.NameIndex)) return FALSE;
	Eqp24Name.NameRecord.LastIndex=kindposition;
	Eqp24Name.PutRecord(Eqp24Name.NameRecord,Eqp24Kind.KindRecord.NameIndex);
	return TRUE;
}

BOOL Equip24DataManager::DeleteRecord(RPTR index,int16 kind=2)
{
	if(kind==1) DeleteName(index);
	else if(kind==2) DeleteKind(index);
	return TRUE;
}

BOOL Equip24DataManager::DeleteKind(RPTR index)
{
	KIND24 kd24_1,kd24_2;
	NAME24 ne24_1;
	//RPTR temprptr=index;
	//RPTR temprptr1=-1;

	if(!Eqp24Kind.GetRecord(kd24_1,index)) return FALSE;
	if(kd24_1.PrvIndex==-1&&kd24_1.NextIndex==-1){
		Eqp24Name.DeleteRecord(kd24_1.NameIndex);
		Eqp24Kind.DeleteRecord(index);
		return TRUE;
	}
	else if(kd24_1.PrvIndex==-1&&kd24_1.NextIndex!=-1){
		Eqp24Name.GetRecord(ne24_1,kd24_1.NameIndex);
		ne24_1.FirstIndex=kd24_1.NextIndex;
		Eqp24Name.PutRecord(ne24_1,kd24_1.NameIndex);
		Eqp24Kind.GetRecord(kd24_2,kd24_1.NextIndex);
		kd24_2.PrvIndex=-1;
		Eqp24Kind.PutRecord(kd24_2,kd24_1.NextIndex);
		Eqp24Kind.DeleteRecord(index);
		return TRUE;
	}
	else if(kd24_1.PrvIndex!=-1&&kd24_1.NextIndex==-1){
		Eqp24Name.GetRecord(ne24_1,kd24_1.NameIndex);
		ne24_1.LastIndex=kd24_1.PrvIndex;
		Eqp24Name.PutRecord(ne24_1,kd24_1.NameIndex);
		Eqp24Kind.GetRecord(kd24_2,kd24_1.PrvIndex);
		kd24_2.NextIndex=-1;
		Eqp24Kind.PutRecord(kd24_2,kd24_1.PrvIndex);
		Eqp24Kind.DeleteRecord(index);
		return TRUE;
	}
	else {
		Eqp24Kind.GetRecord(kd24_2,kd24_1.PrvIndex);
		kd24_2.NextIndex=kd24_1.NextIndex;
		Eqp24Kind.PutRecord(kd24_2,kd24_1.PrvIndex);
		Eqp24Kind.GetRecord(kd24_2,kd24_1.NextIndex);
		kd24_2.PrvIndex=kd24_1.PrvIndex;
		Eqp24Kind.PutRecord(kd24_2,kd24_1.NextIndex);
		Eqp24Kind.DeleteRecord(index);
		return TRUE;
	}
}

BOOL Equip24DataManager::DeleteName(RPTR index)
{
	RPTR temprptr=-1;
	RPTR temprptr1=0;

	if(!Eqp24Name.GetRecord(index)) return FALSE;
	temprptr=Eqp24Name.NameRecord.FirstIndex;
	Eqp24Name.DeleteRecord(index);

	while(temprptr!=-1){
		Eqp24Kind.GetRecord(temprptr);
		temprptr1=temprptr;
		temprptr=Eqp24Kind.KindRecord.NextIndex;
		Eqp24Kind.DeleteRecord(temprptr1);
	}
	return TRUE;
}
