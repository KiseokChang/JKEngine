#include "jkedit.h"
#include "recogwin.h"
#include "recogmsg.h"

RecognitionWindow::RecognitionWindow():
	JKWindow(JKRect(0,0,639,479),0)
{
	WinAttr|=WA_FIRSTSHOW|WA_TITLEMOVEABLE;
	BackColor=Black;
}

RecognitionWindow::~RecognitionWindow()
{}

void RecognitionWindow::SetupWindow()
{
	JKRect rect;
	rect = ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.right-=200;
	AddControl(new JKEdit(rect,0));
}

void RecognitionWindow::EvShowWindow()
{
	JKWindow::EvShowWindow();
	SetFocus();
}

ProcessResult RecognitionWindow::RespondMessage(JKMSG& msg)
{
	switch(msg.main){
		case FINISH_STROKEINPUT:
			FinishStrokeInput();
			break;
		case FINISH_CHARACTERINPUT:
			FinishCharacterInput();
			break;
		default:
			break;
	}
	return JKWindow::RespondMessage(msg);
}


void RecognitionWindow::FinishStrokeInput()
{
	if(GlobalStroke.ProcessInput()){
		JKListBox *listbox =(JKListBox *)GetControl(ID_LISTBOX+1);
		if(listbox) {
			char buf[20];
			sprintf(buf,"%5d",GlobalStroke.DataCode);
			listbox->Add(-1,new HuboBase(buf,0));
			listbox->RedrawWindow();
		}
		GlobalGesture.AddObject(GlobalStroke.Duplicate());
	}
}

void RecognitionWindow::FinishCharacterInput()
{
	unsigned strkcount=GlobalGesture.NodeCount;
	if(strkcount){
		JKListBox *listbox =(JKListBox *)GetControl(ID_LISTBOX+1);
		if(listbox){
			listbox->DestroyAll();
			listbox->RedrawWindow();
		}
		JKControl *ctrl =GetControl(ID_INPUTBOARD);
		if(ctrl) ctrl->RedrawClient();
		listbox =(JKListBox *)GetControl(ID_LISTBOX+2);
		if(listbox){
			char buf[5];
			sprintf(buf,"%4d",strkcount);
			listbox->Add(-1,new HuboBase(buf,0));
			listbox->RedrawWindow();
		}
		ProcessStroke();
		GlobalGesture.ClearNode();
	}
}

void RecognitionWindow::ProcessStroke()
{
	uint32 count=GlobalGesture.NodeCount;
	while(count) count--;
}
