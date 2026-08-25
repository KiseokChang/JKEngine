#include "evnthand.h"

EventHandler::EventHandler(JKControl *main):
	MainWindow(main),CreationError(FALSE),LoopBreak(FALSE)
{
	if(!MainWindow) CreationError=TRUE;
}

WORD EventHandler::Run()
{
	JKMSG msg;

	if(CreationError||MainWindow->CreationError) return ID_CANCEL;
	LoopBreak=FALSE;
	if(!MainWindow->IsInit) MainWindow->InitWindow();
	while(1){
		if(MsgFilter->PatchMessage()){
			MsgFilter->TranslateMessage();
		}
		while(MsgQue->GetMessage(msg)) {
			if(!PreProcessMessage(msg)) {
				LoopBreak=TRUE;
				break;
			}
			MsgFilter->MessageNormalize(*MsgQue);
			if(LoopBreak) break;
		}
		if(LoopBreak) break;
	}
	return TRUE;
}

ProcessResult EventHandler::PreRespondMessage(JKMSG& msg)
{
	switch(msg.main){
		case DESTROYSUBWINDOW:
			if(msg.sub==MainWindow->WinID) return PROCESS_FINISH;
			break;
		case SHOWCURSOR:
			MouseEvents->ShowCursor();
			return PROCESS_SKIP;
		case HIDECURSOR:
			MouseEvents->HideCursor();
			return PROCESS_SKIP;
		case APPEXIT:
			return PROCESS_FINISH;
		default:
			break;
	}
	return PROCESS_OK;
}

BOOL EventHandler::PreProcessMessage(JKMSG msg)
{
	JKControl *win=MainWindow;
	if(!win) return FALSE;
	if(msg.winid) win=(JKControl *)msg.winid;
	else {
		if(msg.main>=ONLYMOVE && msg.main<NCONLYMOVE){
			JKPoint p=JKPoint(msg.sub);
			if(MsgFilter->CaptureWinID){
				msg.winid=MsgFilter->CaptureWinID;
				p=((JKControl *)msg.winid)->ScreenToClient(p);
				msg.sub=p.GetDWORD();
			}
			else msg.main+=NCONLYMOVE-ONLYMOVE;
		}
		ProcessResult result=PreRespondMessage(msg);
		if(result==PROCESS_SKIP) return TRUE;
		else if(result==PROCESS_FINISH) return FALSE;
	}
	win->ProcessMessage(msg);
	return TRUE;
}