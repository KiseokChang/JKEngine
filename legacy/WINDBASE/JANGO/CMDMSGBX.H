#ifndef _CMDMSGBX_H
#define _CMDMSGBX_H

#include "control.h"

class ComandMessageBox:public JKControl
{
public:
	ComandMessageBox(JKRect& rect,WORD ctlid,WORD cmd,JKControl *parent=0);
	void PaintClient(JKDC& dc,JKRect& rect);
	void EvShowWindow();

	WORD Command;
};
#endif