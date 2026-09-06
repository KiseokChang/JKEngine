#ifndef _RECTQUE_H
#define _RECTQUE_H

#include "tmpltdll.h"
#include "jkpoint.h"

class RectContainer
{
public:
	RectContainer(JKRect rect=JKRect()):
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

	JKRect Rect;
	RectContainer *prev,*next;
};

class RectQue : public DLLFrame<RectContainer>
{
public:
	RectQue();
 ~RectQue();

	BOOL GetRect(JKRect& rect,BOOL isdel=TRUE);
	BOOL GetRectLast(JKRect& rect,BOOL isdel=TRUE);

	BOOL PutRect(JKRect& rect);
	BOOL PutRectFirst(JKRect& rect);

	uint32 MinusRect(JKRect minus);
	uint32 MinusRect(RectQue& minus);
//	uint32 PlusRect(JKRect plus);
//	uint32 PlusRect(RectQue& plus);
	uint32 MinimizeCount();
	uint32 ClippingRegion(JKRect rect);
	RectQue *RestRegion(JKRect& rect);
	uint32 AppendRectQue(RectQue& plus);

	RectQue *Duplicate();
	void Offset(JKPoint& p);
	RectQue *OffsetBy(JKPoint& p);
};
#endif