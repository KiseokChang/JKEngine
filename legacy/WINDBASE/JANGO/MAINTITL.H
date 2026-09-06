#ifndef _MAINTITL_H
#define _MAINTITL_H

#include "control.h"
#include "jkscrndc.h"

class MainTitle:public JKControl
{
public:
	MainTitle(JKRect& rect,WORD ctlid,JKControl *parent=0);
	JKRect CalcClientRect();
	void InitWindow();
	void SetupWindow();
	void PaintWindow(JKDC& dc,JKRect& paintrect);
	void PaintClient(JKDC& dc,JKRect& paintrect);

	ProcessResult RespondMessage(JKMSG& msg);

	void ChangeWholeAmount(uint16 ganbu,uint16 sabyeong);
	void ChangeSagoAmount(uint16 ganbu,uint16 sabyeong);
	void ChangeCurrentAmount(uint16 ganbu,uint16 sabyeong);
	void ChangeManagerMode(BOOL mode);
	void ChangeDate();
};
#endif