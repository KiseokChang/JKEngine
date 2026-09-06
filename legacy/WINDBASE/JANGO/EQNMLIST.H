#ifndef _EQNMLIST_H
	#define _EQNMLIST_H

#include "eq24name.h"
#include "scrllist.h"

class EquipNameListBox:public ScrollListBox
{
public:
	EquipNameListBox(JKRect& rect,Equip24Name& eqp24name,char *budae,char *object,WORD ctlid,JKControl *parent=0);
	~EquipNameListBox();
	void InitWindow();

	void InitListBox();
	void MakeString(NAME24& name24,RPTR index,int16 realcount);
	ProcessResult RespondMessage(JKMSG& msg);
	void FindListBox(NAME24& name24,WORD compkind);
	BOOL MakeString2(NAME24& name24,RPTR index,int16 realcount);
//  BOOL Add(char *str);
//  BOOL Destroy(int16 index);
//	BOOL DestoryAll();
public:
	Equip24Name& Eqp24Name;
	char *ObjectDiscern;
	char *BudaeDiscern;
};
#endif