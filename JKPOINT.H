#ifndef JKPOINT_H
#define JKPOINT_H

#include <stdlib.h>
#include "keydef.h"

#define ADJ_START			1
#define ADJ_END				2
#define ADJ_CENTER		3
#define ADJ_PREV			4
#define ADJ_NEXT			5

#define ADJ_LEFT			0x01
#define ADJ_RIGHT			0x02
#define ADJ_XCENTER		0x03
#define ADJ_XPREV			0x04
#define ADJ_XNEXT			0x05
#define ADJ_TOP				0x10
#define ADJ_BOTTOM		0x20
#define ADJ_YCENTER		0x30
#define ADJ_YPREV			0x40
#define ADJ_YNEXT			0x50

#define ADJ_XYCENTER	(ADJ_XCENTER | ADJ_YCENTER)

typedef struct {
	int16 x;
	int16 y;
} POINT;

typedef struct {
	int16 left;
	int16 top;
	int16 right;
	int16 bottom;
} RECT;

class JKPoint
{
public:
	int16 x;
	int16 y;

	JKPoint(){ Set(); }
	JKPoint(int16 xx,int16 yy){ Set(xx,yy); }
	JKPoint(DWORD p){ Set(p); }
	JKPoint(POINT& p){ Set(p); }
	JKPoint(JKPoint& p){ Set(p); }
	~JKPoint(){};

	void Set(){	x=0; y=0; }
	void Set(int16 xx,int16 yy){ x=xx; y=yy; }
	void Set(DWORD p){ x=LOWORD(p); y=HIWORD(p); }
	void Set(POINT& p){	x=p.x; y=p.y; }
	void Set(JKPoint& p){	x=p.x; y=p.y; }

	DWORD GetDWORD() { return MAKEDWORD(x,y); }

	JKPoint operator+(JKPoint& p)
	{
		return JKPoint(x+p.x,y+p.y);
	}

	JKPoint operator-(JKPoint& p)
	{
		return JKPoint(x-p.x,y-p.y);
	}

	JKPoint operator-()
	{
		return JKPoint(-x,-y);
	}

	JKPoint& operator+=(JKPoint& other)
	{
		x+=other.x;
		y+=other.y;
		return *this;
	}

	JKPoint& operator-=(JKPoint& other)
	{
		x-=other.x;
		y-=other.y;
		return *this;
	}

	BOOL operator==(JKPoint& other)
	{
		if(x==other.x && y==other.y) return TRUE;
		return FALSE;
	}

	BOOL operator !=(JKPoint& other)
	{
		return !(*this==other);
	}

	JKPoint& operator=(const JKPoint& other)
	{
		x	= other.x;
		y		= other.y;
		return *this;
	}

	int16 Size(){ return y-x; }
	BOOL IsEmpty(){	return (x==y)? TRUE:FALSE;}

	BOOL IsContain(int16 p);
	BOOL IsContain(JKPoint& p);
	BOOL IsContained(JKPoint& p);
	BOOL IsTouch(JKPoint& p);
	JKPoint CalcPlusRegion(JKPoint& p);
	int16 Adjust(BYTE setting,int16 size);
};


class JKRect
{
public:
	int16 left;
	int16 top;
	int16 right;
	int16 bottom;

	JKRect() { Set(); }
	JKRect(int16 l,int16 t,int16 r,int16 b){ Set(l,t,r,b); }
	JKRect(DWORD l1,DWORD  l2) { Set(l1,l2); }
	JKRect(JKPoint& p1,JKPoint& p2)	{ Set(p1,p2); }
	JKRect(RECT& rt) { Set(rt); }
	JKRect(JKRect& rt) { Set(rt); }

 ~JKRect(){}

	void Set(){left=top=right=bottom=0;}

	void Set(int16 l,int16 t,int16 r,int16 b)
	{
		left=l; right=r;
		top=t; bottom=b;
	}

	void Set(DWORD l1,DWORD  l2)
	{
		int x1=LOWORD(l1),y1=HIWORD(l1),x2=LOWORD(l2),y2=HIWORD(l2);
		left=x1; right=x2;
		top=y1; bottom=y2;
	}

	void Set(JKPoint& p1,JKPoint& p2)
	{
		left=p1.x; right=p2.x;
		top=p1.y; bottom=p2.y;
	}

	void Set(RECT& rt)
	{
		left=rt.left; top=rt.top;	right=rt.right;	bottom=rt.bottom;
	}
	void Set(JKRect& rt)
	{
		left=rt.left; top=rt.top;	right=rt.right;	bottom=rt.bottom;
	}

	JKPoint LeftTop(){ return JKPoint(left,top); }
	JKPoint LeftBottom(){ return JKPoint(left,bottom); }
	JKPoint RightTop(){ return JKPoint(right,top); }
	JKPoint RightBottom(){ return JKPoint(right,bottom); }
	JKPoint WidthHeight(){ return JKPoint(right-left,bottom-top); }
	JKPoint Center(){ return JKPoint((right+left)/2,(bottom+top)/2); }
	int16 Width(){ return right-left; }
	int16 Height(){ return bottom-top; }

	void SetLeftTop(JKPoint p=JKPoint(0,0));
	JKRect SetLeftTopBy(JKPoint p=JKPoint(0,0));
	BOOL IsValid()
	{
		return (left>right || top>bottom)? FALSE:TRUE;
	}
	BOOL IsEmpty(){
		return (left==right || top==bottom)? TRUE:FALSE;
	}
	BOOL IsContain(JKPoint& p);
	BOOL IsContain(JKRect& rect);
	BOOL IsContained(JKRect& rect);
	BOOL IsTouch(JKRect& rect);


	JKPoint Adjust(BYTE setting,JKPoint size);

	BOOL operator==(JKRect& other)
	{
		if(left==other.left && top==other.top
			&& right==other.right && bottom==other.bottom) return TRUE;
		return FALSE;
	}

	BOOL operator !=(JKRect& other)
	{
		return !(*this==other)? TRUE:FALSE;
	}

	JKRect& operator=(JKRect& other)
	{
		left	= other.left;
		top		= other.top;
		right	= other.right;
		bottom	= other.bottom;
		return *this;
	}

	JKRect& Offset(JKPoint& p)
	{
		left+=p.x;	top+=p.y;	right+=p.x;	bottom+=p.y;
		return *this;
	}

	JKRect OffsetBy(JKPoint& p)
	{
		return JKRect(left+p.x,top+p.y,right+p.x,bottom+p.y);
	}

	JKRect& Expand(int16 delta)
	{
		left-=delta;	top-=delta;	right+=delta;	bottom+=delta;
		return *this;
	}

	JKRect ExpandBy(int16 delta)
	{
		return JKRect(left-delta,top-delta,right+delta,bottom+delta);
	}

	JKRect operator-()
	{
		return JKRect(-left,-top,-right,-bottom);
	}

	BOOL CalcInnerLine(JKPoint& p1,JKPoint& p2);
	BOOL CalcInnerHLine(JKPoint& p,uint16& width);
	BOOL CalcInnerVLine(JKPoint& p,uint16& height);
	JKRect CalcPlusRegion(JKRect& rect);
	JKRect CalcOverlap(JKRect& rect);
};
#endif