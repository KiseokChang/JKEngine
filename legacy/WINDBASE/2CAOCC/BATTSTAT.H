#ifndef _BATTSTAT_H
#define _BATTSTAT_H

#include "control.h"

class BattalionStatusControl:public JKControl
{
public:
	BattalionStatusControl(JKRect rect,JKControl *parent=0);
 ~BattalionStatusControl();

	void InitWindow();
	JKRect CalcClientRect();
	void SetupWindow();
	void PaintWindow(JKDC& dc,JKRect& paintrect);
	void PaintClient(JKDC& dc,JKRect& paintrect);

	void EvCtrlRightClick(WORD ctrlid);

	void EvCommand(WORD cmdid,WORD,DWORD,WORD);
	void EvTimerHappen(DWORD happencount);

	void EvFireBegin();
	void EvFireFinish(uint16 index);

	int16 GetOrder(char budaename[6]);
	BOOL PrintFireCommand(FireRecord& rec);
};
#endif
