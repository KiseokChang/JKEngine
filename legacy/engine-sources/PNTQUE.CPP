#include "pntque.h"

PointQue::PointQue():
	DLLFrame<PointContainer>()
{}

PointQue::~PointQue()
{}

BOOL PointQue::PutPoint(JKPoint& point)
{
	PointContainer *container = new PointContainer(point);
	return PutNodeLast(container);
}

BOOL PointQue::PutPointFirst(JKPoint& point)
{
	PointContainer *container = new PointContainer(point);
	return PutNodeFirst(container);
}

BOOL PointQue::GetPoint(JKPoint& point,BOOL isdel)
{
	PointContainer *container=GetNodeFirst(isdel);
	if(container){
		point=container->Point;
		if(isdel) delete container;
		return TRUE;
	}
	return FALSE;
}

BOOL PointQue::GetPointLast(JKPoint& point,BOOL isdel)
{
	PointContainer *container=GetNodeLast(isdel);
	if(container){
		point=container->Point;
		if(isdel) delete container;
		return TRUE;
	}
	return FALSE;
}

uint32 PointQue::AppendPointQue(PointQue& plus)
{
	return AppendDLLFrame(plus);
}

//====================================================================

PointQue *PointQue::Duplicate()
{
	PointQue *temp = new PointQue();
	if(temp) *temp=*this;
	return temp;
}

void PointQue::Offset(JKPoint& p)
{
	PointContainer *container = HeadNode;
	while(container) {
		container->Point+=p;
		container=container->next;
	}
}

PointQue *PointQue::OffsetBy(JKPoint& p)
{
	PointQue *temp=Duplicate();
	if(temp) temp->Offset(p);
	return temp;
}

BOOL PointQue::GetRect(JKRect& rect)
{
	if(!NodeCount) return FALSE;
	rect.Set(HeadNode->Point,HeadNode->Point);
	PointContainer *container = HeadNode;
	while(container) {
		if(rect.left>container->Point.x) rect.left=container->Point.x;
		if(rect.right<container->Point.x) rect.right=container->Point.x;
		if(rect.top>container->Point.y) rect.top=container->Point.y;
		if(rect.bottom<container->Point.y) rect.bottom=container->Point.y;
		container=container->next;
	}
	return TRUE;
}
