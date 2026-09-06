#ifndef _LISTBOX_H
#define _LISTBOX_H

#include "huboman.h"
#include "control.h"

class JKListBox:public JKControl, public HuboManager
{
public:

	JKListBox(JKRect& pos,WORD ctlid,JKControl *parent=0);
 ~JKListBox();

	JKRect CalcClientRect();

	void PaintWindow(JKDC& dc,JKRect& rect);
	void PaintClient(JKDC& dc,JKRect& rect);

	void EvSetFocus(DWORD lostid);
	void EvKillFocus(DWORD getid);

	void EvCharDown(WORD key,WORD modifier);
	void EvKeyDown(WORD key,WORD modifier);
	void EvLeftClick(JKPoint& p);
	void EvLeftDragging(JKPoint& p);
	void EvLeftPressing(JKPoint& p);
	void EvLeftRelease(JKPoint&);

	BOOL DestroyAll();
	BOOL Destroy(int16 index);

	BOOL ChangePosition(JKPoint pos);
	void DrawEntry(JKDC& dc,int16 sel);
	int16  FindSelectEntry(JKPoint p);

	virtual BOOL SetCurrentIndex(int16 sel);

	int16 CurrentIndex;

	virtual uint16 SetFirstIndex(uint16 index);
	uint16 FirstIndex;
	uint16 ShowEntryCount;
	BOOL IsSelecting;
	BOOL IsLBSelectEntry;
};
#endif
