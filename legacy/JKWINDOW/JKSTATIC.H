#ifndef _JKSTATIC_H
#define _JKSTATIC_H

#include "control.h"

class JKStatic:public JKControl
{
public:
	JKStatic(JKRect& rect,WORD ctlid,JKControl *parent=0);
	void PaintClient(JKDC& dc,JKRect& rect);
	BYTE AdjustFlag;
};
#endif