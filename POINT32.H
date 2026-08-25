#ifndef POINT32_H
#define POINT32_H

#include "jkpoint.h"

typedef struct {
	int32 x;
	int32 y;
} POINT32;

typedef struct {
	int32 left;
	int32 top;
	int32 right;
	int32 bottom;
} RECT32;

class JKPoint32
{
public:
	int32 x;
	int32 y;

	JKPoint32(){ Set(); }
	JKPoint32(int32 xx,int32 yy){ Set(xx,yy); }
	JKPoint32(POINT32& p){ Set(p); }
	JKPoint32(JKPoint32& p){ Set(p); }
	JKPoint32(JKPoint& p){ Set(p); }
	JKPoint32(POINT& p){ Set(p); }
	~JKPoint32(){};

	void Set(){	x=0; y=0; }
	void Set(int32 xx,int32 yy){ x=xx; y=yy; }
	void Set(POINT32& p){	x=p.x; y=p.y; }
	void Set(JKPoint32& p){	x=p.x; y=p.y; }
	void Set(POINT& p){	x=p.x; y=p.y; }
	void Set(JKPoint& p){	x=p.x; y=p.y; }


	JKPoint32 operator+(JKPoint32& p)
	{
		return JKPoint32(x+p.x,y+p.y);
	}

	JKPoint32 operator-(JKPoint32& p)
	{
		return JKPoint32(x-p.x,y-p.y);
	}

	JKPoint32 operator-()
	{
		return JKPoint32(-x,-y);
	}

	JKPoint32& operator+=(JKPoint32& other)
	{
		x+=other.x;
		y+=other.y;
		return *this;
	}

	JKPoint32& operator-=(JKPoint32& other)
	{
		x-=other.x;
		y-=other.y;
		return *this;
	}

	BOOL operator==(JKPoint32& other)
	{
		if(x==other.x && y==other.y) return TRUE;
		return FALSE;
	}

	BOOL operator !=(JKPoint32& other)
	{
		return !(*this==other);
	}

	JKPoint32& operator=(const JKPoint32& other)
	{
		x	= other.x;
		y		= other.y;
		return *this;
	}

	int32 Size(){ return y-x; }
	BOOL IsEmpty(){	return (x==y)? TRUE:FALSE;}

	BOOL IsContain(int32 p);
	BOOL IsContain(JKPoint32& p);
	BOOL IsContained(JKPoint32& p);
	BOOL IsTouch(JKPoint32& p);
	JKPoint32 CalcPlusRegion(JKPoint32& p);
	int32 Adjust(BYTE setting,int32 size);
};


class JKRect32
{
public:
	int32 left;
	int32 top;
	int32 right;
	int32 bottom;

	JKRect32() { Set(); }
	JKRect32(int32 l,int32 t,int32 r,int32 b){ Set(l,t,r,b); }
	JKRect32(JKPoint32& p1,JKPoint32& p2)	{ Set(p1,p2); }
	JKRect32(RECT32& rt) { Set(rt); }
	JKRect32(JKRect32& rt) { Set(rt); }
	JKRect32(RECT& rt) { Set(rt); }
	JKRect32(JKRect& rt) { Set(rt); }

 ~JKRect32(){}

	void Set(){left=top=right=bottom=0;}

	void Set(int32 l,int32 t,int32 r,int32 b)
	{
		left=l; right=r;
		top=t; bottom=b;
	}

	void Set(JKPoint32& p1,JKPoint32& p2)
	{
		left=p1.x; right=p2.x;
		top=p1.y; bottom=p2.y;
	}

	void Set(RECT32& rt)
	{
		left=rt.left; top=rt.top;	right=rt.right;	bottom=rt.bottom;
	}
	void Set(JKRect32& rt)
	{
		left=rt.left; top=rt.top;	right=rt.right;	bottom=rt.bottom;
	}

	void Set(RECT& rt)
	{
		left=rt.left; top=rt.top;	right=rt.right;	bottom=rt.bottom;
	}
	void Set(JKRect& rt)
	{
		left=rt.left; top=rt.top;	right=rt.right;	bottom=rt.bottom;
	}

	JKPoint32 LeftTop(){ return JKPoint32(left,top); }
	JKPoint32 LeftBottom(){ return JKPoint32(left,bottom); }
	JKPoint32 RightTop(){ return JKPoint32(right,top); }
	JKPoint32 RightBottom(){ return JKPoint32(right,bottom); }
	JKPoint32 WidthHeight(){ return JKPoint32(right-left,bottom-top); }
  JKPoint32 Center(){ return JKPoint32((right+left)/2,(bottom+top)/2); }
	int32 Width(){ return right-left; }
	int32 Height(){ return bottom-top; }

	void SetLeftTop(JKPoint32 p=JKPoint32(0,0));
	JKRect32 SetLeftTopBy(JKPoint32 p=JKPoint32(0,0));
	BOOL IsValid()
	{
		return (left>right || top>bottom)? FALSE:TRUE;
	}
	BOOL IsEmpty(){
		return (left==right || top==bottom)? TRUE:FALSE;
	}
	BOOL IsContain(JKPoint32& p);
	BOOL IsContain(JKRect32& rect);
	BOOL IsContained(JKRect32& rect);
	BOOL IsTouch(JKRect32& rect);


	JKPoint32 Adjust(BYTE setting,JKPoint32 size);

	BOOL operator==(JKRect32& other)
	{
		if(left==other.left && top==other.top
			&& right==other.right && bottom==other.bottom) return TRUE;
		return FALSE;
	}

	BOOL operator !=(JKRect32& other)
	{
		return !(*this==other)? TRUE:FALSE;
	}

	JKRect32& operator=(JKRect32& other)
	{
		left	= other.left;
		top		= other.top;
		right	= other.right;
		bottom	= other.bottom;
		return *this;
	}

	JKRect32& Offset(JKPoint32& p)
	{
		left+=p.x;	top+=p.y;	right+=p.x;	bottom+=p.y;
		return *this;
	}

	JKRect32 OffsetBy(JKPoint32& p)
	{
		return JKRect32(left+p.x,top+p.y,right+p.x,bottom+p.y);
	}

	JKRect32& Expand(int32 delta)
	{
		left-=delta;	top-=delta;	right+=delta;	bottom+=delta;
		return *this;
	}

	JKRect32 ExpandBy(int32 delta)
	{
		return JKRect32(left-delta,top-delta,right+delta,bottom+delta);
	}

	JKRect32 operator-()
	{
		return JKRect32(-left,-top,-right,-bottom);
	}

	BOOL CalcInnerLine(JKPoint32& p1,JKPoint32& p2);
	BOOL CalcInnerHLine(JKPoint32& p,uint32& width);
	BOOL CalcInnerVLine(JKPoint32& p,uint32& height);
	JKRect32 CalcPlusRegion(JKRect32& rect);
	JKRect32 CalcOverlap(JKRect32& rect);
};
#endif