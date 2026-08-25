#ifndef _RECT32QU_H
#define _RECT32QU_H

#include "tmpltdll.h"
#include "point32.h"

class RectContainer
{
public:
	RectContainer(JKRect32 rect=JKRect32()):
		prev(0),next(0),Rect(rect)
	{}

	BOOL IsEqual(RectContainer *node,uint16 /*compkind*/)
	{
		if(node)
			return Rect==node->Rect;
		return FALSE;
	}

	RectContainer *Duplicate()
	{	return new RectContainer(Rect);	}

	JKRect32 Rect;
	RectContainer *prev,*next;
};

class Rect32Que : public DLLFrame<RectContainer>
{
public:
	Rect32Que();
 ~Rect32Que();

	BOOL GetRect(JKRect32& rect,BOOL isdel=TRUE);
	BOOL GetRectLast(JKRect32& rect,BOOL isdel=TRUE);

	BOOL PutRect(JKRect32& rect);
	BOOL PutRectFirst(JKRect32& rect);

	uint32 MinusRect(JKRect32 minus);
	uint32 MinusRect(Rect32Que& minus);
//	uint32 PlusRect(JKRect32 plus);
//	uint32 PlusRect(Rect32Que& plus);
	uint32 MinimizeCount();
	uint32 ClippingRegion(JKRect32 rect);
	Rect32Que *RestRegion(JKRect32& rect);
	uint32 AppendRectQue(Rect32Que& plus);

	Rect32Que *Duplicate();
	void Offset(JKPoint32& p);
	Rect32Que *OffsetBy(JKPoint32& p);
};
#endif
