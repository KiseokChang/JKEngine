#ifndef _PNTQUE_H
#define _PNTQUE_H

#include "tmpltdll.h"
#include "jkpoint.h"

class PointContainer
{
public:
	PointContainer(JKPoint point=JKPoint()):
		prev(0),next(0),Point(point)
	{}

	BOOL IsEqual(PointContainer *node,uint16 /*compkind*/)
	{
		if(node)
			return Point==node->Point;
		return FALSE;
	}

	PointContainer *Duplicate()
	{	return new PointContainer(Point);	}

	JKPoint Point;
	PointContainer *prev,*next;
};

class PointQue : public DLLFrame<PointContainer>
{
public:
	PointQue();
 ~PointQue();

	BOOL GetPoint(JKPoint& point,BOOL isdel=TRUE);
	BOOL GetPointLast(JKPoint& point,BOOL isdel=TRUE);

	BOOL PutPoint(JKPoint& point);
	BOOL PutPointFirst(JKPoint& point);

	uint32 AppendPointQue(PointQue& plus);

	PointQue *Duplicate();
	void Offset(JKPoint& p);
	PointQue *OffsetBy(JKPoint& p);

  BOOL GetRect(JKRect& rect);
};
#endif