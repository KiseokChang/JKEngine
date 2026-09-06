#ifndef _SHOWBRD_H
#define _SHOWBRD_H

#include "control.h"

class ShowBoard:public JKControl
{
public:
	ShowBoard(JKRect& rect,WORD ctlid,BYTE*& buf,JKControl *parent=0);
	void PaintClient(JKDC& dc,JKRect& paintrect);
	BYTE*& ImageBuff;
};
#endif