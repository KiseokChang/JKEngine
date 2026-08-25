#ifndef _VSHOWBLK_H
#define _VSHOWBLK_H

#include "real2pix.h"
#include "control.h"

class VBlockShow:public JKControl
{
public:
	VBlockShow(JKRect& rect,WORD ctlid,JKPoint32& blksize,RealCoordManager& coordman);
	void PaintClient(JKDC& dc,JKRect& rect);

	JKPoint32& BlockSize;
	RealCoordManager& RealCoordMan;
};
#endif
