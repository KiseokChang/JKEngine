#include "point32.h"

BOOL JKPoint32::IsContain(int32 p)
{
	return (x<=p && p<=y)? TRUE:FALSE;
}
BOOL JKPoint32::IsContain(JKPoint32& p)
{
	return (x<=p.x && y>=p.y)? TRUE:FALSE;
}

BOOL JKPoint32::IsContained(JKPoint32& p)
{
	return (x>=p.x && y<=p.y)? TRUE:FALSE;
}

BOOL JKPoint32::IsTouch(JKPoint32& p)
{
	return (IsContain(p.x)||IsContain(p.y)||p.IsContain(x)||p.IsContain(y))? TRUE:FALSE;
}

int32 JKPoint32::Adjust(BYTE setting,int32 size)
{
	int32 pos;
	int32 width=y-x;
	switch(setting){
		case ADJ_START:
			pos = x;
			break;
		case ADJ_END:
			pos = y-size;
			break;
		case ADJ_CENTER:
			pos = x+(width-size)/2;
			break;
		case ADJ_PREV:
			pos = x-(size+1);
			break;
		case ADJ_NEXT:
			pos = y+1;
			break;
		default:
			pos = x;
			break;
	}
	return pos;
}

JKPoint32 JKPoint32::CalcPlusRegion(JKPoint32& p)
{
	return JKPoint32((x<p.x)? x:p.x,(y>p.y)? y:p.y);
}

//***************************************************************//

BOOL JKRect32::IsContain(JKPoint32& p)
{
	return (left<=p.x && p.x<=right && top<=p.y && p.y<=bottom)? TRUE:FALSE;
}
BOOL JKRect32::IsContain(JKRect32& rect)
{
	return (left<=rect.left && right>=rect.right && top<=rect.top && bottom>=rect.bottom)? TRUE:FALSE;
}

BOOL JKRect32::IsContained(JKRect32& rect)
{
	return (left>=rect.left && right<=rect.right && top>=rect.top && bottom<=rect.bottom)? TRUE:FALSE;
}

BOOL JKRect32::IsTouch(JKRect32& rect)
{
	JKPoint32 h1,h2,v1,v2;
	h1=JKPoint32(left,right); h2=JKPoint32(rect.left,rect.right);
	v1=JKPoint32(top,bottom); v2=JKPoint32(rect.top,rect.bottom);
	return (h1.IsTouch(h2) && v1.IsTouch(v2))? TRUE:FALSE;
}

void JKRect32::SetLeftTop(JKPoint32 p)
{
	Offset(-LeftTop());
	Offset(p);
}

JKRect32 JKRect32::SetLeftTopBy(JKPoint32 p)
{
	JKRect32 rect(*this);
	rect.Offset(-LeftTop());
	rect.Offset(p);
	return rect;
}

BOOL JKRect32::CalcInnerLine(JKPoint32&,JKPoint32&)
{
	return TRUE;
}

BOOL JKRect32::CalcInnerHLine(JKPoint32& p,uint32& width)
{
	if(width && JKPoint32(top,bottom).IsContain(p.y)){
		JKPoint32 temp(p.x,p.x+width-1);
		temp.Set((left>temp.x)? left:temp.x,(right<temp.y)? right:temp.y);
		if(temp.x<=temp.y){
			p.x=temp.x;
			width=temp.Size()+1;
			return TRUE;
		}
	}
	return FALSE;
}

BOOL JKRect32::CalcInnerVLine(JKPoint32& p,uint32& height)
{
	if(height && JKPoint32(left,right).IsContain(p.x)){
		JKPoint32 temp(p.y,p.y+height-1);
		temp.Set((top>temp.x)? top:temp.x,(bottom<temp.y)? bottom:temp.y);
		if(temp.x<=temp.y){
			p.y=temp.x;
			height=temp.Size()+1;
			return TRUE;
		}
	}
	return FALSE;
}

JKRect32 JKRect32::CalcPlusRegion(JKRect32& rect)
{
	return JKRect32(min(left,rect.left),min(top,rect.top),max(right,rect.right),max(bottom,rect.bottom));
}

JKRect32 JKRect32::CalcOverlap(JKRect32& rect)
{
	return JKRect32(max(left,rect.left),max(top,rect.top),min(right,rect.right),min(bottom,rect.bottom));
}

JKPoint32 JKRect32::Adjust(BYTE setting,JKPoint32 size)
{
	JKPoint32 p;
	JKPoint32 Temp;

	Temp.Set(left,right);
	p.x=Temp.Adjust(BYTE(setting&0x0f),size.x);
	Temp.Set(top,bottom);
	p.y=Temp.Adjust(BYTE((setting>>4)&0x0f),size.y);
	return p;
}