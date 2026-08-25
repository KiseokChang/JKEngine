#ifndef _JOAPPABT_H
#define _JOAPPABT_H

#include "control.h"

class JOAppAboutControl:public JKControl
{
public:
	JOAppAboutControl(JKRect& rect,WORD ctrlid,WORD resid=0,JKControl *parent=0);
 ~JOAppAboutControl();

	JKRect CalcClientRect();
	void PaintWindow(JKDC& dc,JKRect& paintrect);
	void PaintClient(JKDC& dc,JKRect& paintrect);
	void DrawAbout(JKDC& dc);

	void SetResourceID(WORD resid);

	WORD ResourceID;
};

#endif