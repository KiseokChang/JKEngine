#include "jkstatic.h"

JKStatic::JKStatic(JKRect& rect,WORD ctlid,JKControl *parent):
	JKControl(rect,ctlid,parent),AdjustFlag(0x00)
{}

void JKStatic::PaintClient(JKDC& dc,JKRect&)
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	dc.SolidBar(rect,BackColor);
	BOOL overwrite=dc.IsOverWrite;
	dc.IsOverWrite=FALSE;
	dc.SetTextColor(TextColor);
	dc.TextOutX(rect,WinText,AdjustFlag);
	dc.IsOverWrite=overwrite;
}
