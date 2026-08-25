#ifndef _STATBAR_H
#define _STATBAR_H

#include "jkwindow.h"

class StatusBar:public JKControl
{
public:
	StatusBar(JKRect rect,WORD ctrlid,JKControl *parent=0);
 ~StatusBar();

	void InitWindow();
	JKRect CalcClientRect();
	void SetupWindow();
	void PaintWindow(JKDC& dc,JKRect& paintrect);
	void PaintClient(JKDC& dc,JKRect& paintrect);

	ProcessResult RespondMessage(JKMSG& msg);
	void ChangeDate();

};
#endif