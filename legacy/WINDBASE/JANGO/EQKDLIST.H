#ifndef _EQKDLIST_H
	#define _EQKDLIST_H

#include "eq24kind.h"
#include "eq24totl.h"
#include "eq24name.h"
#include "scrllist.h"

class EquipKindListBox:public ScrollListBox
{
public:
	EquipKindListBox(JKRect& rect,Equip24Kind& eqp24kind,Equip24Name& eqp24name,char *obj,WORD ctlid,JKControl *parent=0);
	~EquipKindListBox();
	void InitWindow();

	void InitListBox();
	void MakeString(KIND24& kind24,RPTR index,int16 realcount);
	void EquipKindListBox::ShowString(RPTR index);
	void InitTotal();
	void ShowCKind(RPTR index);
//  BOOL Add(char *str);
//  BOOL Destroy(int16 index);
//	BOOL DestoryAll();
public:
	Equip24Kind& Eqp24Kind;
	Equip24Name& Eqp24Name;
	char *ObjectDiscern;
	Equip24Manager Equip24TotalMan;
};
#endif