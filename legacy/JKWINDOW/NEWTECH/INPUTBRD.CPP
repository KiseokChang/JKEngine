#include "recogmsg.h"
#include "inputbrd.h"

static JKPoint OldPoint;

InputBoard::InputBoard(JKRect rect,WORD ctrlid,JKControl *parent):
	JKWindow(rect,parent,ctrlid),IsStartInput(FALSE)
{
	BackColor=LtGray;
	WinAttr|=WA_FIRSTSHOW;
	WinForm=0x01;
	Depth=-1;
}

void InputBoard::EvLeftClick(JKPoint& p)
{
	if(!IsStartInput){
		IsStartInput=TRUE;
		GlobalStroke.ClearNode();
		p=ClientToScreen(p);
		OldPoint=p;
		GlobalStroke.AddPoint(JKPoint32(p));
	}
}

void InputBoard::EvLeftDragging(JKPoint& p)
{
	if(IsStartInput){
		p=ClientToScreen(p);

		MouseEvents->HideCursor();
		JKDC *dc=CreateScreenDC();
		if(dc) dc->Line(OldPoint,p,Black);
		MouseEvents->ShowCursor();
		OldPoint=p;
		GlobalStroke.AddPoint(JKPoint32(p));
	}
}

void InputBoard::EvLeftRelease(JKPoint&)
{
	if(IsStartInput)
		SendMessage(FINISH_INPUT);
}

void InputBoard::EvRightClick(JKPoint&)
{
	if(!IsStartInput)
		SendControlMessage(FINISH_CHARACTERINPUT);
}

void InputBoard::EvNcLeftDragging(JKPoint&)
{
	if(IsStartInput)
		SendMessage(CANCEL_INPUT);
}
void InputBoard::EvNcLeftRelease(JKPoint&)
{
	if(IsStartInput)
		SendMessage(CANCEL_INPUT);
}

ProcessResult InputBoard::RespondMessage(JKMSG& msg)
{
	switch(msg.main){
		case FINISH_INPUT:
			IsStartInput=FALSE;
			if(GlobalStroke.NodeCount)
				SendControlMessage(FINISH_STROKEINPUT);
			break;
		case CANCEL_INPUT:
			IsStartInput=FALSE;
			GlobalStroke.ClearNode();
			break;
		default:
			break;
	}
	return JKWindow::RespondMessage(msg);
}
