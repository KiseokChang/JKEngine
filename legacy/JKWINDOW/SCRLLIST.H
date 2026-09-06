#ifndef _SCRLLIST_H
#define _SCRLLIST_H

#include "listbox.h"

class ScrollListBox:public JKListBox
{
public:
	ScrollListBox(JKRect rect,WORD ctrlid,JKControl *parent=0);
	~ScrollListBox(){}

	void InitWindow();
	void SetupWindow();
	JKRect CalcClientRect();
	void EvSBScrollUp(WORD ctrlid,WORD curpos);
	void EvSBScrollDown(WORD ctrlid,WORD curpos);
	void EvSBScrollJump(WORD ctrlid,WORD curpos);

	BOOL Add(uint16 index,HuboBase *hubo);
	BOOL DestroyAll();
	BOOL Destroy(int16 index);
	uint16 SetFirstIndex(uint16 index);
	void SetDragScroll(BOOL flag);
	BOOL DragScrollFlag;
};
#endif