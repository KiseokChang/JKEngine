#ifndef _SHOWPRFX_H
#define _SHOWPRFX_H

#include "real2pix.h"
#include "jkbutton.h"

class PrefixShow:public JKButton
{
public:
	PrefixShow(JKRect& rect,WORD ctlid,RealCoordManager& coordman);
	void PaintClient(JKDC& dc,JKRect& rect);

	RealCoordManager& RealCoordMan;
};
#endif
