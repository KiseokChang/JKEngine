#ifndef _TESTWIN_H
#define _TESTWIN_H

#include "memdc.h"
#include "jkwindow.h"

class TestWindow:public JKWindow
{
public:
	TestWindow();
 ~TestWindow();
	void InitWindow();
	void SetupWindow();
	void EvShowWindow();
	void PaintClient(JKDC& dc,JKRect& rect);

	void EvKeyDown(WORD key,WORD);
	void EvDeadKeyDown(WORD key,WORD);
	void EvButtonClick(WORD ctrlid);
	void EvRightClick(JKPoint&);

	void EvMoveWindow(JKRect );
	void EvResizeWindow(JKRect );
	void EvPlaceForward();
	void EvPlaceBackward();

	void EvTimerHappen(DWORD happencount);

	BYTE ClientColor[2];
	JKMemoryDC *MemDC;
};

#endif