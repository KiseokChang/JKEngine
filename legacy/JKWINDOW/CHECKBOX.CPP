#include "checkbox.h"

JKCheckBox::JKCheckBox(JKRect& rect,WORD ctlid,JKControl *parent):
	JKControl(rect,ctlid,parent),Status(FALSE),IsAutoControl(TRUE)
{
	WinRect.bottom=WinRect.top+17;
	CursorID=1;
}

JKRect JKCheckBox::CalcClientRect()
{
	JKRect rect =WinRect.SetLeftTopBy(JKPoint(0,0));
	rect.Expand(-1);
	rect.right = rect.left+ 15;
	rect.bottom= rect.top + 15;
	return rect;
}

void JKCheckBox::PaintWindow(JKDC& dc,JKRect&)
{
	JKRect rect=WinRect.SetLeftTopBy(JKPoint(0,0));
	dc.SolidBar(rect,BackColor);
	if(rect.Width()>16){
		rect.left+=16;
		if(WinText) {
			BOOL overwrite=dc.IsOverWrite;
			dc.IsOverWrite=FALSE;
			dc.SetTextColor(TextColor);
			dc.TextOutX(rect,WinText,ADJ_YCENTER);
			dc.IsOverWrite=overwrite;
			dc.Rectangle(rect,TextColor);
		}
	}
}

void JKCheckBox::PaintClient(JKDC& dc,JKRect&)
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	dc.Box3D(rect,-1,LtGray);
	if(HasFocus()) dc.Rectangle3D(rect,-1,LtBlue,Blue);
	if(Status)
		dc.SolidBar(JKRect(rect.left+3,rect.top+3,rect.right-3,rect.bottom-3),Black);
}

void JKCheckBox::EvLeftClick(JKPoint& )
{
	if(IsAutoControl){
		Status=!Status;
		SendControlMessage(CHECK_TOGGLE,Status);
		RedrawClient();
	}
}

void JKCheckBox::EvCharDown(WORD key,WORD)
{
	if(key==' ') SendMessage(LEFTCLICK);
}

void JKCheckBox::EvSetFocus(DWORD)
{
	RedrawClient();
}
void JKCheckBox::EvKillFocus(DWORD)
{
	RedrawClient();
}
