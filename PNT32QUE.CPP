#include "pnt32que.h"

Point32Que::Point32Que():
	DLLFrame<Point32Container>()
{}

Point32Que::~Point32Que()
{}

BOOL Point32Que::PutPoint(JKPoint32& point)
{
	Point32Container *container = new Point32Container(point);
	return PutNodeLast(container);
}

BOOL Point32Que::PutPointFirst(JKPoint32& point)
{
	Point32Container *container = new Point32Container(point);
	return PutNodeFirst(container);
}

BOOL Point32Que::GetPoint(JKPoint32& point,BOOL isdel)
{
	Point32Container *container=GetNodeFirst(isdel);
	if(container){
		point=container->Point;
		if(isdel) delete container;
		return TRUE;
	}
	return FALSE;
}

BOOL Point32Que::GetPointLast(JKPoint32& point,BOOL isdel)
{
	Point32Container *container=GetNodeLast(isdel);
	if(container){
		point=container->Point;
		if(isdel) delete container;
		return TRUE;
	}
	return FALSE;
}

uint32 Point32Que::AppendPointQue(Point32Que& plus)
{
	return AppendDLLFrame(plus);
}

//====================================================================

Point32Que *Point32Que::Duplicate()
{
	Point32Que *temp = new Point32Que();
	if(temp) *temp=*this;
	return temp;
}

void Point32Que::Offset(JKPoint32& p)
{
	Point32Container *container = HeadNode;
	while(container) {
		container->Point+=p;
		container=container->next;
	}
}

Point32Que *Point32Que::OffsetBy(JKPoint32& p)
{
	Point32Que *temp=Duplicate();
	if(temp) temp->Offset(p);
	return temp;
}

BOOL Point32Que::GetRect(JKRect32& rect)
{
	if(!NodeCount) return FALSE;
	rect.Set(HeadNode->Point,HeadNode->Point);
	Point32Container *container = HeadNode;
	while(container) {
		if(rect.left>container->Point.x) rect.left=container->Point.x;
		if(rect.right<container->Point.x) rect.right=container->Point.x;
		if(rect.top>container->Point.y) rect.top=container->Point.y;
		if(rect.bottom<container->Point.y) rect.bottom=container->Point.y;
		container=container->next;
	}
	return TRUE;
}
