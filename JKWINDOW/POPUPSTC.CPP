#include "popupstc.h"

PopupStatic::PopupStatic(JKRect& rect,WORD ctlid,JKControl *parent):
	JKStatic(rect,ctlid,parent)
{
	WinAttr&=~BYTE(WA_FIRSTSHOW);
	CursorID=0;
}

PopupStatic::~PopupStatic()
{}

JKRect PopupStatic::CalcClientRect()
{
	JKRect rect = WinRect.SetLeftTopBy(JKPoint(0,0));
	rect.Expand(-1);
	return rect;
}

void PopupStatic::PaintWindow(JKDC& dc,JKRect&)
{
	JKRect rect=WinRect.SetLeftTopBy(JKPoint(0,0));
	dc.Rectangle(rect,Black);
}
