#ifndef _SPIN_H
#define _SPIN_H

#include "control.h"

class Spin:public JKControl
{
public:
	Spin(JKRect& rect,WORD ctlid,JKControl *parent=0);

	JKRect CalcClientRect();
	void PaintWindow(JKDC& dc,JKRect& rect);
	void PaintClient(JKDC& dc,JKRect& rect);

	void EvLeftClick(JKPoint& p);
	void EvLeftPressing(JKPoint& p);
	void EvLeftRelease(JKPoint&);
	void EvNcLeftRelease(JKPoint&);

	JKRect  CalcUpRect();
	JKRect  CalcDownRect();
	BOOL IsActive;
};
#endif