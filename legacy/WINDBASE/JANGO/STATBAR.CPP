#include "statbar.h"

StatusBar::StatusBar(JKRect rect,WORD ctrlid,JKControl *parent):
	JKControl(rect,ctrlid,parent)
{
	BackColor=LtGray;
	WinAttr|=WA_FIRSTSHOW;
}

StatusBar::~StatusBar()
{
}

JKRect StatusBar::CalcClientRect()
{
	JKRect rect =WinRect.SetLeftTopBy(JKPoint(0,0));
	rect.Expand(-5);
	return rect;
}

void StatusBar::PaintWindow(JKDC& dc,JKRect&)
{
	JKRect rect =WinRect.SetLeftTopBy(JKPoint(0,0));
	dc.Box3D(rect,1);
	rect.Expand(-4);
	dc.Rectangle3D(rect,-1);
}

void StatusBar::PaintClient(JKDC& dc,JKRect&)
{
	JKRect rect =ClientRect.SetLeftTopBy(JKPoint(0,0));
	dc.SetTextColor(TextColor);
	dc.SetBackColor(BackColor);
	dc.TextOutX(rect,WinText,ADJ_YCENTER);
}

