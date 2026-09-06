#ifndef _CORDVIEW_H
#define _CORDVIEW_H

#include "real2pix.h"
#include "jkedit.h"

class CoordView:public JKEdit
{
public:
	CoordView(JKRect& rect,WORD ctlid,RealCoordManager& coordman);
	void SetCoord(JKPoint32 coord);

	RealCoordManager& RealCoordMan;
};

#endif
