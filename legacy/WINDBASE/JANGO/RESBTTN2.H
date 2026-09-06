#ifndef _RESBTTN2_H
#define _RESBTTN2_H
#include "resbttn.h"

class JKResButton2:public JKResButton
{
public:
	JKResButton2(JKRect& rect,WORD ctlid,WORD resid,JKControl *parent=0);
	void PaintClient(JKDC& dc,JKRect& rect);

	int16 Depth2;
};

#endif