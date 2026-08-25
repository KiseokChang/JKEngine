#include "pointchn.h"

PointChain::PointChain(JKPoint32&  p,WORD refid,WORD objtype):
	ChainObject(refid,objtype),DataPoint(p)
{
}

PointChain::~PointChain()
{
}

BOOL PointChain::IsEqual(ChainObject *node,WORD compkind)
{
	if(ChainObject::IsEqual(node,compkind)){
		if(compkind&0x08 && DataPoint!=((PointChain *)node)->DataPoint) return FALSE;
		return TRUE;
	}
	return FALSE;
}

ChainObject *PointChain::Duplicate()
{
	return new PointChain(DataPoint,ReferenceID);
}