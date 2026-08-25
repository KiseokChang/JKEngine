#ifndef _MOONCTRL_H
#define _MOONCTRL_H

#include "sollucvt.h"
#include "jkbutton.h"

class MoonControl:public JKButton
{
public:
	MoonControl(JKRect& rect,WORD ctlid,JKControl *parent=0);

	void PaintClient(JKDC& dc,JKRect& rect);
  void EvTimerHappen(DWORD /*happencount*/);
  struct time CurrentTime;
};
#endif
