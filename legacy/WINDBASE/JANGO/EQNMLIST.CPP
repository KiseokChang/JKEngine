#include "eqnmlist.h"
#include "string.h"
#include <mem.h>

EquipNameListBox::EquipNameListBox(JKRect& rect,Equip24Name& eqp24name,char *budae,char *object,WORD ctlid,JKControl *parent):
	ScrollListBox(rect,ctlid,parent),Eqp24Name(eqp24name),BudaeDiscern(budae),ObjectDiscern(object)
{
	//WinAttr&=~BYTE(WA_FIRSTSHOW);
}

EquipNameListBox::~EquipNameListBox()
{
	DestroyAll();
}

void EquipNameListBox::InitWindow()
{
	ScrollListBox::InitWindow();
	InitListBox();
}

void EquipNameListBox::InitListBox()
{
	RPTR index=1;
	RPTR tempindex=-1;
	int16  realcount=0;
	WORD compkind=0x0003;
	NAME24 tempname24;

	strcpy(tempname24.Division,ObjectDiscern);
	strcpy(tempname24.Attached,BudaeDiscern);
	if(!strcmp(BudaeDiscern,BudaeName)) compkind=0x0001;

	DestroyAll();
	while(index<Eqp24Name.DFileMan.FileHeader[Eqp24Name.FilePoint].Record_Count) {
		tempindex=Eqp24Name.Search(tempname24,index,compkind);
		if(tempindex!=-1) {
			Eqp24Name.GetRecord(tempindex);
			MakeString(Eqp24Name.NameRecord,tempindex,realcount);
			realcount++;
			index=tempindex;
		}
		index++;
	}
	Add(realcount,new HuboBase(" ** µa‹¡ˆa {·³“¡”a **",-1));
}

void EquipNameListBox::MakeString(NAME24& name24,RPTR index,int16 realcount)
{
	char buf[30];
	if(!strcmp(ObjectDiscern,"·©¤e")){
		Add(realcount,new HuboBase("·©¤e¢‰¸a",(WORD)index));
	}
	else {
		sprintf(buf," %-8s %-11s",name24.Name,name24.Number);
		Add(realcount,new HuboBase(buf,(WORD)index));
	}
}


ProcessResult EquipNameListBox::RespondMessage(JKMSG& msg)
{
	return JKListBox::RespondMessage(msg);
}

void EquipNameListBox::FindListBox(NAME24& name24,WORD compkind)
{
	WORD realcount=0;
	RPTR index=1,tempindex;

	while(index<Eqp24Name.DFileMan.FileHeader[Eqp24Name.FilePoint].Record_Count) {
		tempindex=Eqp24Name.Search(name24,index,compkind);
		if(tempindex!=-1) {
			Eqp24Name.GetRecord(tempindex);
			MakeString(Eqp24Name.NameRecord,tempindex,realcount);
			realcount++;
			index=tempindex;
		}
		index++;
	}
	Add(realcount,new HuboBase(" ** µa‹¡ˆa {·³“¡”a **",-1));
}

BOOL EquipNameListBox::MakeString2(NAME24& name24,RPTR index,int16 realcount)
{
	HuboBase *hubo=HuboManager::HeadNode;

	char buf[30];
	if(!strcmp(ObjectDiscern,"·©¤e")){
		Add(realcount,new HuboBase("·©¤e¢‰¸a",(WORD)index));
	}
	else {
		sprintf(buf," %-8s %-11s",name24.Name,name24.Number);
		while(hubo){
			if(!strcmp(hubo->TextBuff,buf)) return FALSE;
			hubo=hubo->next;
		}
		Add(realcount,new HuboBase(buf,(WORD)index));
		return TRUE;
	}
	return FALSE;
}

