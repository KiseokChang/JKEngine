#ifndef _JKWINDOW_H
#define _JKWINDOW_H

#include "control.h"

class JKWindow:public JKControl
{
public:
	JKWindow(JKRect rect,JKControl *parent=0,WORD ctrlid=0);
 ~JKWindow();

	void PaintWindow(JKDC& dc,JKRect& rect);

	virtual void DrawSystemBox(JKDC& dc,JKRect rect);
	virtual void DrawTitleBar(JKDC& dc,JKRect rect);

	void EvElLeftClick(WORD element);

	ProcessResult RespondMessage(JKMSG& msg);

	JKRect CalcClientRect();

	virtual BOOL GetRegion(WORD element,JKRect& rect);
	int16 FindWindowElement(JKPoint& p);

	int16 Depth;
	int16 BorderSize;
	BYTE WinForm;

	int16 BorderColor;
	int16 TitleColor;
	int16 TitleBkColor;
	int16 NFTitleColor;
	int16 NFTitleBkColor;
};
#endif