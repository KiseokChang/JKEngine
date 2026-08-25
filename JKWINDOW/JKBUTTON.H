#ifndef _JKBUTTON_H
#define _JKBUTTON_H

#include "jkstatic.h"

class JKButton:public JKStatic
{
public:
	JKButton(JKRect& rect,WORD ctlid,JKControl *parent=0);

	void PaintWindow(JKDC& dc,JKRect& rect);
	void PaintClient(JKDC& dc,JKRect& rect);

	void EvSetFocus(DWORD lostid);
	void EvKillFocus(DWORD getid);
	void EvLeftClick(JKPoint& );
	void EvLeftRelease(JKPoint& );
	void EvNcLeftRelease(JKPoint& );
	void EvLeftDragging(JKPoint& );
	void EvLeftPressing(JKPoint& p);
	void EvNcLeftDragging(JKPoint& );
	void EvCharDown(WORD key,WORD);
	JKRect CalcClientRect();

	BOOL Status;
	BOOL Selecting;
	int16 Depth;

	void SetEnable(BOOL flag);
	BOOL IsEnable;
};
#endif