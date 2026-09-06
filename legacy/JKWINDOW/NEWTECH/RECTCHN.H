#ifndef _RECTCHN_H
#define _RECTCHN_H

#include "point32.h"
#include "chainobj.h"

#define CO_RECT		2

class RectChain : public ChainObject
{
public:
	RectChain(JKRect32&  rect,WORD refid=0,WORD objtype=CO_RECT);
 ~RectChain();
	BOOL IsEqual(ChainObject *node,WORD compkind);
	ChainObject *Duplicate();

	JKRect32 DataRect;
};
#endif



