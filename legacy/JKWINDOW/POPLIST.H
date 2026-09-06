#ifndef _POPLIST_H
#define _POPLIST_H

#include "huboman.h"
#include "control.h"

class PopupListBox:public JKControl
{
public:

	PopupListBox(JKRect& rect,WORD ctlid,HuboManager *huboman=0,JKControl *parent=0);
 ~PopupListBox();

	JKRect CalcClientRect();
	void PaintWindow(JKDC& dc,JKRect& rect);
	void PaintClient(JKDC& dc,JKRect& rect);

	void EvSetFocus(DWORD lostid);
	void EvKillFocus(DWORD getid);

	void EvShowWindow();

	void EvCharDown(WORD key,WORD modifier);
	void EvKeyDown(WORD key,WORD modifier);
	void EvLeftClick(JKPoint& p);
	void EvLeftDragging(JKPoint& p);
	void EvLeftPressing(JKPoint& p);
	void EvLeftRelease(JKPoint&);


	BOOL Add(uint16 index,HuboBase *hubo);
	BOOL DestroyAll();
	BOOL Destroy(int16 index);

	BOOL ChangePosition(JKPoint pos);
	void DrawEntry(JKDC& dc,int16 sel);
	int16  FindSelectEntry(JKPoint p);
	uint16 GetHuboCount(){ return HuboMan? HuboMan->NodeCount:0;}

	virtual BOOL SetCurrentIndex(int16 sel);
	virtual uint16 SetFirstIndex(uint16 index);

	WORD MaxTextWidth;
	int16 CurrentIndex;
	uint16 FirstIndex;
	uint16 ShowEntryCount;
	BOOL IsSelecting;

	BOOL Ownership;
	HuboManager *HuboMan;
};
#endif