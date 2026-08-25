#ifndef _STROKCHN_H
#define _STROKCHN_H

#include "rectchn.h"

#define CO_STROKE		3

class StrokeChain : public RectChain
{
public:
	StrokeChain(JKRect32&  rect,WORD refid=0,WORD objtype=CO_STROKE);
 ~StrokeChain();

	void AddPoint(JKPoint32& p);

	JKPoint32 CalcThreshold01(uint32 width,uint32 height);
	uint32 Resize_Filter(JKRect32 normal);
	int32 FindStroke();
	BOOL ProcessInput();

	int32 DataCode;
};
#endif


