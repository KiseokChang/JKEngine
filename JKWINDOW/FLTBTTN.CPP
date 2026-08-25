#include "fltbttn.h"

JKFloatingButton::JKFloatingButton(JKRect& rect,WORD ctlid,JKControl *parent):
	JKButton(rect,ctlid,parent),ShadowColor(Gray),BaseColor(LtGray)
{
	Depth=4;
	BackColor=White;
}

JKRect JKFloatingButton::CalcClientRect()
{
	JKRect rect = WinRect.SetLeftTopBy(JKPoint(0,0));
	WORD depth=abs(Depth);
	rect.left+=depth;
	rect.top+=depth;
	if(!Status) {
		if(Depth>=0) rect.SetLeftTop(JKPoint(0,0));
		else rect.SetLeftTop(JKPoint(depth,depth));
	}
	else{
		if(Depth>=0) rect.SetLeftTop(JKPoint(depth,depth));
		else rect.SetLeftTop(JKPoint(0,0));
	}
	return rect;
}

void JKFloatingButton::PaintWindow(JKDC& dc,JKRect&)
{
	JKRect rect = WinRect.SetLeftTopBy(JKPoint(0,0));
	dc.SolidBar(rect,BaseColor);
	int16 depth=abs(Depth);
	rect.left+=depth;
	rect.top+=depth;
	JKRect rect1,rect2;
	if(Depth>=0) rect2=rect.SetLeftTopBy(JKPoint(depth,depth));
	else rect2=rect.SetLeftTopBy(JKPoint(0,0));
	if(!Status) {
		if(Depth>=0) rect1=rect.SetLeftTopBy(JKPoint(0,0));
		else rect1=rect.SetLeftTopBy(JKPoint(depth,depth));
	}
	else{
		if(Depth>=0) rect1=rect.SetLeftTopBy(JKPoint(depth,depth));
		else rect1=rect.SetLeftTopBy(JKPoint(0,0));
	}
	dc.SolidBar(rect2,ShadowColor);
	dc.SolidBar(rect1,BackColor);
}

void JKFloatingButton::PaintClient(JKDC& dc,JKRect&)
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	dc.SolidBar(rect,HasFocus()? Yellow:BackColor);
	if(WinText) {
		BOOL overwrite=dc.IsOverWrite;
		dc.IsOverWrite=FALSE;
		dc.SetTextColor(TextColor);
		dc.TextOutX(rect,WinText,AdjustFlag,TRUE);
		dc.IsOverWrite=overwrite;
	}
}

void JKFloatingButton::EvLeftClick(JKPoint& p)
{
	JKButton::EvLeftClick(p);
	ClientRect=CalcClientRect();
}

void JKFloatingButton::EvLeftRelease(JKPoint& p)
{
	JKButton::EvLeftRelease(p);
	ClientRect=CalcClientRect();
}

void JKFloatingButton::EvNcLeftRelease(JKPoint& p)
{
	JKButton::EvNcLeftRelease(p);
	ClientRect=CalcClientRect();
}

void JKFloatingButton::EvLeftDragging(JKPoint& p)
{
	JKButton::EvLeftDragging(p);
	ClientRect=CalcClientRect();
}

void JKFloatingButton::EvNcLeftDragging(JKPoint& p)
{
	JKButton::EvNcLeftDragging(p);
	ClientRect=CalcClientRect();
}
