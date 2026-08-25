#ifndef _HSHOWBLK_H
#define _HSHOWBLK_H

#include "real2pix.h"
#include "control.h"

class HBlockShow:public JKControl
{
public:
	HBlockShow(JKRect& rect,WORD ctlid,JKPoint32& blksize,RealCoordManager& coordman);
	void PaintClient(JKDC& dc,JKRect& rect);

	JKPoint32& BlockSize;
	RealCoordManager& RealCoordMan;
};

#endif
