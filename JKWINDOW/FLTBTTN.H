#ifndef _FLTBTTN_H
#define _FLTBTTN_H

#include "jkbutton.h"

class JKFloatingButton:public JKButton
{
public:
	JKFloatingButton(JKRect& rect,WORD ctlid,JKControl *parent=0);

	JKRect CalcClientRect();
	void PaintWindow(JKDC& dc,JKRect& rect);
  void PaintClient(JKDC& dc,JKRect&);

	void EvLeftClick(JKPoint& );
	void EvLeftRelease(JKPoint& );
	void EvNcLeftRelease(JKPoint& );
	void EvLeftDragging(JKPoint& );
	void EvNcLeftDragging(JKPoint& );

	int16 ShadowColor;
	int16 BaseColor;
};
#endif