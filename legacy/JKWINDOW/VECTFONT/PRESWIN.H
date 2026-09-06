#ifndef _PRESWIN_H
#define _PRESWIN_H

#include "jkwindow.h"

class PresentWindow:public JKWindow
{
public:
	PresentWindow();
 ~PresentWindow();
	void EvShowWindow();
	void PaintClient(JKDC& dc,JKRect& rect);
	void EvTimerHappen(DWORD /*happencount*/);
	void EvResizeWindow(JKRect );

	JKPoint TxtCenter;
	int16 TxtColor;
	int16 TxtCount;

};

#endif