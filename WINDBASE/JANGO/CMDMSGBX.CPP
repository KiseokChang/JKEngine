#include "cmdmsgbx.h"

ComandMessageBox::ComandMessageBox(JKRect& rect,WORD ctlid,WORD cmd,JKControl *parent):
	JKControl(rect,ctlid,parent),Command(cmd)
{}

void ComandMessageBox::PaintClient(JKDC& dc,JKRect&)
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	dc.SolidBar(rect,BackColor);
	BOOL overwrite=dc.IsOverWrite;
	dc.IsOverWrite=FALSE;
	dc.SetTextColor(TextColor);
	dc.TextOutX(rect,WinText,ADJ_XYCENTER);
	dc.IsOverWrite=overwrite;
}

void ComandMessageBox::EvShowWindow()
{
	JKControl::EvShowWindow();
	SendControlCommand(Command);
	CloseWindow();
}