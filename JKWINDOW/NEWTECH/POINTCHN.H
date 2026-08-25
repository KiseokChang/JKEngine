#ifndef _POINTCHN_H
#define _POINTCHN_H

#include "point32.h"
#include "chainobj.h"

#define CO_POINT		1

class PointChain : public ChainObject
{
public:
	PointChain(JKPoint32&  p,WORD refid=0,WORD objtype=CO_POINT);
 ~PointChain();
	BOOL IsEqual(ChainObject *node,WORD compkind);
	ChainObject *Duplicate();

	JKPoint32 DataPoint;
};
#endif


