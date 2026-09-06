#ifndef _OCCWIN_H
#define _OCCWIN_H

#include "firerec.h"
#include "jkwindow.h"

class OCCWindow:public JKWindow
{
public:
	OCCWindow();
 ~OCCWindow();

	JKRect CalcClientRect();

	void InitWindow();
	void SetupWindow();
	void EvShowWindow();
	void PaintWindow(JKDC&dc,JKRect&);

	void EvCommand(WORD cmdid,WORD ,DWORD ,WORD );

	void EvTimerHappen(DWORD happencount);
	void EvFirePlanTarget();
	void EvFireBegin();
	void EvFireFinish(uint16 index);
	BOOL PrintFireCommand(FireRecord& rec);

	BOOL PrepareFire(uint16 type);
	BOOL EvFireProcess(uint16 type);

	void SendCommandBox(char *message,WORD command);
};

#endif