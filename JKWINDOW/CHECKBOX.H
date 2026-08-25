#ifndef _CHECKBOX_H
#define _CHECKBOX_H

#include "control.h"

class JKCheckBox:public JKControl
{
public:
	JKCheckBox(JKRect& rect,WORD ctlid,JKControl *parent=0);
	JKRect CalcClientRect();
	void PaintWindow(JKDC& dc,JKRect& rect);
	void PaintClient(JKDC& dc,JKRect& rect);
	void EvLeftClick(JKPoint& );
	void EvCharDown(WORD key,WORD);
	void EvSetFocus(DWORD);
	void EvKillFocus(DWORD);

	BOOL Status;
	BOOL IsAutoControl;
};
#endif
