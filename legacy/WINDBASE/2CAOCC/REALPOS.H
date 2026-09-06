#ifndef _REALPOS_H
#define _REALPOS_H

#include "real2pix.h"

class RealPosition
{
public:
	RealPosition();
	virtual ~RealPosition();

	BOOL IsValidCoord(JKPoint32& coord);
	BOOL IsNearCoord(JKPoint32& p,uint32 delta);

	virtual BOOL SetCoord(JKPoint32& pos);
	virtual BOOL GetCoord(JKPoint32& pos);
};
#endif

