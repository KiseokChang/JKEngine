#include "eqkdlist.h"
#include "string.h"
#include <mem.h>

EquipKindListBox::EquipKindListBox(JKRect& rect,Equip24Kind& eqp24Kind,Equip24Name& eqp24name,char *obj,WORD ctlid,JKControl *parent):
	ScrollListBox(rect,ctlid,parent),Eqp24Kind(eqp24Kind),Eqp24Name(eqp24name),ObjectDiscern(obj)
{
}

EquipKindListBox::~EquipKindListBox()
{
	DestroyAll();
}

void EquipKindListBox::InitWindow()
{
	ScrollListBox::InitWindow();
}

void EquipKindListBox::InitListBox()
{
	RPTR index=1;
	int16  realcount=0;

	while(index<Eqp24Kind.DFileMan.FileHeader[Eqp24Kind.FilePoint].Record_Count) {
		if(Eqp24Kind.GetRecord(index)){
			MakeString(Eqp24Kind.KindRecord,index,realcount);
			realcount++;
		}
		index++;
	}
	Add(realcount,new HuboBase(" *****  µa‹¡ˆa {·³“¡”a  *****"));
}

void EquipKindListBox::MakeString(KIND24& kind24,RPTR index,int16 realcount)
{
	char buf[100];
	WORD boyou=kind24.A+kind24.B+kind24.C;
	sprintf(buf,"%5u %-14s  %-19s %5u  %5u  %5u  %5u  %5u",kind24.Order,kind24.Name,kind24.Number,kind24.Inga,boyou,kind24.A,kind24.B,kind24.C);
	Add(realcount,new HuboBase(buf,(WORD)index));
}

void EquipKindListBox::ShowString(RPTR index)
{
	int16  realcount=0;
	DestroyAll();

	Eqp24Name.GetRecord(index);
	index=Eqp24Name.NameRecord.FirstIndex;
	while(Eqp24Kind.GetRecord(index)&&index!=-1L) {
		MakeString(Eqp24Kind.KindRecord,index,realcount);
		index=Eqp24Kind.KindRecord.NextIndex;
		realcount++;
	}
	Add(realcount,new HuboBase(" *****  µa‹¡ˆa {·³“¡”a  *****",-1));
}

void EquipKindListBox::InitTotal()
{
	RPTR index=1;
	DestroyAll();
	Equip24TotalMan.ClearNode();
	while(Eqp24Kind.DFileMan.FileHeader[Eqp24Kind.FilePoint].Record_Count>index){
		if(Eqp24Kind.GetRecord(index)){
			Equip24TotalMan.Add(Eqp24Kind.KindRecord);
		}
		index++;
	}
	index=0;
	Equip24Base* base;
	while(index<Equip24TotalMan.NodeCount){
		base=Equip24TotalMan.GetNode(index);
		MakeString(base->Kind24,index,index);
		index++;
	}
	Add(index,new HuboBase(" *****  µa‹¡ˆa {·³“¡”a  *****",-1));
}

void EquipKindListBox::ShowCKind(RPTR index)
{
	int16  realcount=0;
	DestroyAll();

	Eqp24Name.GetRecord(index);
	index=Eqp24Name.NameRecord.FirstIndex;
	while(Eqp24Kind.GetRecord(index)&&index!=-1L) {
		if(Eqp24Kind.Compare(Eqp24Kind.KindRecord,"",0x0040)) {
			MakeString(Eqp24Kind.KindRecord,index,realcount);
		}
		index=Eqp24Kind.KindRecord.NextIndex;
		realcount++;
	}
	Add(realcount,new HuboBase(" *****  µa‹¡ˆa {·³“¡”a  *****",-1));
}