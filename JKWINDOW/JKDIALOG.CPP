#include "jkmodule.h"
#include "jkapp.h"
#include "jkdialog.h"

JKDialog::JKDialog(JKRect rect,JKControl *parent,WORD ctrlid):
	JKWindow(rect,parent,ctrlid),EventHandler(this),
	ReturnValue(ID_CANCEL),IsModal(TRUE)
{
	IsRemoveable=FALSE;
	WinForm=0x07;
	BackColor=LtGray;
	WinAttr|=WA_FIRSTSHOW|WA_NORESTRICT|WA_CHANGEPLACE;
}

JKDialog::~JKDialog()
{}

void JKDialog::EvShowWindow()
{
	JKWindow::EvShowWindow();
	SetFocus();
}

ProcessResult JKDialog::RespondMessage(JKMSG& msg)
{
	switch(msg.main){
		case ID_OK:
			ReturnValue=ID_OK;
			EvDlgOk();
			CloseWindow();
			break;
		case ID_CANCEL:
			ReturnValue=ID_CANCEL;
			EvDlgCancel();
			CloseWindow();
			break;
		case ID_YES:
			ReturnValue=ID_YES;
			EvDlgYes();
			CloseWindow();
			break;
		case ID_NO:
			ReturnValue=ID_NO;
			EvDlgNo();
			CloseWindow();
			break;
		default:
			break;
	}
	return JKWindow::RespondMessage(msg);
}

WORD JKDialog::Run()
{
	if(EventHandler::CreationError||JKWindow::CreationError) return ID_CANCEL;
	ReturnValue=ID_CANCEL;
	SaveMsgFilter=*MsgFilter;
	MsgFilter->InitFilter();

	JKApplication *mainapp=(JKApplication *)MainApplication;

	if(!Parent||!mainapp->TestSubWindow((DWORD)Parent,TRUE))
		Parent=mainapp;
	Parent->PutNodeLast(this);
	IsRemoveable=FALSE;
	if(!IsInit) InitWindow();


	EventHandler::Run();
	MsgFilter->SetFocus(SaveMsgFilter.FocusWinID);
	MsgFilter->BeginCapture(SaveMsgFilter.CaptureWinID);
	return ReturnValue;
}

BOOL JKDialog::PreProcessMessage(JKMSG msg)
{
	if(!Parent) return FALSE;
	if(msg.main==DESTROYSUBWINDOW && msg.winid==Parent->WinID && msg.sub==WinID){
		JKControl *find = new JKControl(JKRect(),0);
		if(find) {
			find->WinID=WinID;
			Parent->GetNode(find,0x01,TRUE);
			find->WinID=DWORD(find);
			delete find;
			return FALSE;
		}
	}

	JKApplication *mainapp=(JKApplication *)MainApplication;

	if(!msg.winid&&(msg.main>=ONLYMOVE && msg.main<NCONLYMOVE)){
		JKPoint p=JKPoint(msg.sub);
		if(MsgFilter->CaptureWinID){
			msg.winid=MsgFilter->CaptureWinID;
			p=((JKControl *)msg.winid)->ScreenToClient(p);
			msg.sub=p.GetDWORD();
		}
		else {
			msg.main+=NCONLYMOVE-ONLYMOVE;
			if(IsModal){
				p=Parent->ScreenToClient(p);
				if(!WinRect.IsContain(p)){
					MouseEvents->SetCursorID(0);
				}
				p=Parent->ClientToScreen(p);
			}
		}
	}


	if(!IsModal || (msg.winid&&msg.winid!=WinID) || !((msg.main>=CHARDOWN&&msg.main<=DEADKEYDOWN)||(msg.main>=ONLYMOVE&&msg.main<=NCRIGHTDRAGGING)))
		mainapp->ProcessMessage(msg);
	else{
		ProcessMessage(msg);
	}
	return TRUE;
}