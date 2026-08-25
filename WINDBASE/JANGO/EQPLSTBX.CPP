#include "eqplstbx.h"
#include "string.h"

EquipListBox::EquipListBox(JKRect& rect,WORD ctlid,JKControl *parent):
	JKListBox(rect,ctlid,parent)
{
	WinAttr&=~BYTE(WA_FIRSTSHOW);
}

EquipListBox::~EquipListBox()
{
	DestoryAll();
}

BOOL EquipListBox::Add(char *str)
{
  int16 i=0;
	HuboBase *hubo=HuboManager::HeadNode;
	while(hubo){
    if(!strcmp(hubo->TextBuff,str)) return FALSE;
		if(strcmp(hubo->TextBuff,str)>0) return HuboManager::PutNode(new HuboBase(str),i);
		hubo=hubo->next;
    i++;
	}
	return JKListBox::Add(HuboManager::NodeCount,new HuboBase(str));
}

BOOL EquipListBox::Destroy(int16 index)
{
	return JKListBox::Destroy(index);
}

BOOL EquipListBox::DestoryAll()
{
	return JKListBox::DestroyAll();
}