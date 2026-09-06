#ifndef _POPUPSTC_H
#define _POPUPSTC_H

#include "jkstatic.h"

class PopupStatic : public JKStatic
{
public:
	PopupStatic(JKRect& rect,WORD ctlid,JKControl *parent=0);
 ~PopupStatic();
	JKRect CalcClientRect();
	void PaintWindow(JKDC& dc,JKRect& rect);
};
#endif