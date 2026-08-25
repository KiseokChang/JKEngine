#include "realpos.h"

RealPosition::RealPosition()
{}
RealPosition::~RealPosition()
{}

BOOL RealPosition::IsValidCoord(JKPoint32& coord)
{
	return RealCoordManager().IsValidReal(coord);
}

BOOL RealPosition::IsNearCoord(JKPoint32& p,uint32 delta)
{
	JKPoint32 pp;
	if(GetCoord(pp)){
		if(uint32(double(pp.x-p.x)*(pp.x-p.x)+double(pp.y-p.y)*(pp.y-p.y))<delta*delta)
			return TRUE;
	}
	return FALSE;
}

BOOL RealPosition::SetCoord(JKPoint32&)
{
	return FALSE;
}
BOOL RealPosition::GetCoord(JKPoint32&)
{
	return FALSE;
}

