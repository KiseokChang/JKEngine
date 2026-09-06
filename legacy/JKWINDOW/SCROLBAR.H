#ifndef _SCROLBAR_H
#define _SCROLBAR_H

#include "control.h"
#include "jkscrndc.h"

class JKScrollBar:public JKControl
{
public:
	JKScrollBar(JKRect& rect,WORD ctlid,WORD maxcount,WORD curpos=0,JKControl *parent=0);

	JKRect CalcClientRect();
	void SetupWindow();
	void PaintWindow(JKDC& dc,JKRect& rect);
	void PaintClient(JKDC& dc,JKRect& rect);

	void EvLeftClick(JKPoint& p);
	void EvLeftPressing(JKPoint& p);
	void EvLeftDragging(JKPoint& p);
	void EvLeftRelease(JKPoint&);
	void EvNcLeftRelease(JKPoint&);
	void EvButtonClick(WORD ctrlid);
	void EvButtonPressing(WORD ctrlid);

	int16     CalcThumbHeight();
	int16			CalcThumbVPosition();
	JKPoint CalcThumbPosition();
	JKRect  CalcThumbRect();
	JKRect  CalcUpRect();
	JKRect  CalcDownRect();
	void 		ChangeMaxCount(WORD maxcount);
	WORD MaxCount;
	WORD CurrentPosition;
	BOOL DragScrollFlag;
	BOOL IsScrolling;
};
#endif