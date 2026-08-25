#ifndef _MENUBAR_H
#define _MENUBAR_H

#include "control.h"

class MenuBar:public JKControl
{
public:
	MenuBar(JKRect rect,JKControl *parent=0);
 ~MenuBar();

	JKRect CalcClientRect();
	void SetupWindow();
	void PaintWindow(JKDC& dc,JKRect& paintrect);
	void PaintClient(JKDC& dc,JKRect& paintrect);

	void EvButtonClick(WORD ctrlid);
	void EvDeadKeyDown(uint16 key,WORD /*modifier*/);
};
#endif