#include "rectchn.h"

RectChain::RectChain(JKRect32&  rect,WORD refid,WORD objtype):
	ChainObject(refid,objtype),DataRect(rect)
{
}

RectChain::~RectChain()
{
}

BOOL RectChain::IsEqual(ChainObject *node,WORD compkind)
{
	if(ChainObject::IsEqual(node,compkind)){
		if(compkind&0x08 && DataRect!=((RectChain *)node)->DataRect) return FALSE;
		return TRUE;
	}
	return FALSE;
}

ChainObject *RectChain::Duplicate()
{
	return new RectChain(DataRect,ReferenceID);
}