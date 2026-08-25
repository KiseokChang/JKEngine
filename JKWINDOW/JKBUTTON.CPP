#include "jkbutton.h"

JKButton::JKButton(JKRect& rect,WORD ctlid,JKControl *parent):
	JKStatic(rect,ctlid,parent),Depth(1),Selecting(FALSE),Status(FALSE),IsEnable(TRUE)
{
	BackColor=LtGray;
	AdjustFlag=ADJ_XYCENTER;
	CursorID=3;
}

JKRect JKButton::CalcClientRect()
{
	JKRect rect = WinRect.SetLeftTopBy(JKPoint(0,0));
	return rect;
}

void JKButton::PaintWindow(JKDC&/*dc*/,JKRect&)
{}

void JKButton::PaintClient(JKDC& dc,JKRect&)
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	if(!Status) dc.Rectangle3D(rect,Depth);
	else dc.Rectangle3D(rect,-Depth);
	rect.Expand(-abs(Depth));
	dc.SolidBar(rect,HasFocus()? Yellow:BackColor);
	if(WinText) {
		BOOL overwrite=dc.IsOverWrite;
		dc.IsOverWrite=FALSE;
		dc.SetTextColor(TextColor);
		dc.TextOutX(rect,WinText,AdjustFlag,TRUE);
		dc.IsOverWrite=overwrite;
	}
}

void JKButton::EvSetFocus(DWORD )
{
	RedrawClient(FALSE);
}

void JKButton::EvKillFocus(DWORD )
{
	RedrawClient(FALSE);
}

void JKButton::EvLeftClick(JKPoint& )
{
	if(IsEnable && !Selecting) {
		Status=TRUE;
		RedrawWindow();
		Selecting=TRUE;
	}
}

void JKButton::EvLeftRelease(JKPoint& )
{
	if(IsEnable && Selecting) {
		Status=FALSE;
		RedrawWindow();
		SendControlMessage(BUTTON_CLICK);
		Selecting=FALSE;
	}
}

void JKButton::EvNcLeftRelease(JKPoint& )
{
	if(IsEnable && Selecting) {
		if(Status){
			Status=FALSE;
			RedrawWindow();
		}
		Selecting=FALSE;
	}
}

void JKButton::EvLeftDragging(JKPoint& )
{
	if(IsEnable && Selecting){
		if(!Status) {
			Status=TRUE;
			RedrawWindow();
		}
		SendControlMessage(BUTTON_PRESSING);
	}
}

void JKButton::EvLeftPressing(JKPoint& p)
{
	EvLeftDragging(p);
}

void JKButton::EvNcLeftDragging(JKPoint& )
{
	if(IsEnable && Selecting && Status) {
		Status=FALSE;
		RedrawWindow();
	}
}

void JKButton::EvCharDown(WORD key,WORD)
{
	if(key==ENTER) {
		SendMessage(LEFTCLICK);
		SendMessage(LEFTRELEASE);
	}
}


void JKButton::SetEnable(BOOL flag)
{
	IsEnable=flag;
	if(flag) {
		WinAttr&=~BYTE(WA_NOHASFOCUS);
		CursorID=3;
	}
	else {
		WinAttr|=WA_NOHASFOCUS;
		CursorID=0;
	}
}
