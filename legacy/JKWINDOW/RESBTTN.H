#ifndef _RESBTTN_H
#define _RESBTTN_H

#include "jkbutton.h"

class JKResButton:public JKButton
{
public:
	JKResButton(JKRect& rect,WORD ctlid,WORD resid,JKControl *parent=0);

	void PaintClient(JKDC& dc,JKRect& rect);

	virtual void DrawDisable(JKDC& dc);
	virtual void DrawReleased(JKDC& dc);
	virtual void DrawPressed(JKDC& dc);
	virtual void DrawHasFocus(JKDC& dc);
	WORD ResID;
};
#endif