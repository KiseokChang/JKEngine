#ifndef _PNT32QUE_H
#define _PNT32QUE_H

#include "tmpltdll.h"
#include "point32.h"

class Point32Container
{
public:
	Point32Container(JKPoint32 point=JKPoint32()):
		prev(0),next(0),Point(point)
	{}

	BOOL IsEqual(Point32Container *node,uint16 /*compkind*/)
	{
		if(node)
			return Point==node->Point;
		return FALSE;
	}

	Point32Container *Duplicate()
	{	return new Point32Container(Point);	}

	JKPoint32 Point;
	Point32Container *prev,*next;
};

class Point32Que : public DLLFrame<Point32Container>
{
public:
	Point32Que();
 ~Point32Que();

	BOOL GetPoint(JKPoint32& point,BOOL isdel=TRUE);
	BOOL GetPointLast(JKPoint32& point,BOOL isdel=TRUE);

	BOOL PutPoint(JKPoint32& point);
	BOOL PutPointFirst(JKPoint32& point);

	uint32 AppendPointQue(Point32Que& plus);

	Point32Que *Duplicate();
	void Offset(JKPoint32& p);
	Point32Que *OffsetBy(JKPoint32& p);

  BOOL GetRect(JKRect32& rect);
};
#endif