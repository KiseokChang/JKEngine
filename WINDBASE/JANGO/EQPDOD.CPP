#include "jkedit.h"
#include "jkstatic.h"
#include "jkbutton.h"
#include "eqpdod.h"

EqpDOD::EqpDOD(JKRect rect,WORD ctrlid,int16 len,JKControl *parent):
	JKWindow(rect,parent,ctrlid)
{
	if((Buffer=new char[len+1])==NULL) JKWindow::CreationError=TRUE;
	else CreationError=FALSE;
	*Buffer=0;
	Length=len;
	ExplanText=NULL;
	WinForm=0x03;
	BackColor=LtGray;
}

EqpDOD::~EqpDOD()
{
	if(Buffer) delete[] Buffer;
	if(ExplanText) delete[] ExplanText;
}

void EqpDOD::SetupWindow()
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));

	rect.left+=10;
	rect.right-=10;
	rect.top=ClientRect.Height()/3;
	rect.bottom=rect.top+18;
	JKEdit *edit=new JKEdit(rect,ID_EDIT+51,0,Length,FALSE);
	AddControl(edit);

	rect.bottom=rect.top-3;
	rect.top=rect.bottom-20;
	JKStatic *stc=new JKStatic(rect,ID_STATIC+1);
	stc->SetText(ExplanText);
	stc->BackColor=LtGray;
	stc->TextColor=Red;
	AddControl(stc);

	rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.top=rect.bottom-40;
	rect.bottom=rect.top+30;
	rect.right=ClientRect.Width()/2-10;
	rect.left=rect.right-50;

	JKButton *btn=new JKButton(rect,ID_BUTTON+1);
	btn->SetText("ÑÂ ·¥");
	AddControl(btn);

	rect.left=rect.right+20;
	rect.right=rect.left+50;
	btn=new JKButton(rect,ID_BUTTON+2);
	btn->SetText("Âá ­¡");
	AddControl(btn);
}

void EqpDOD::SetExplanText(char *s)
{
	int16 len=strlen(s);
	ExplanText = new char[len+1];
	if(ExplanText) strcpy(ExplanText,s);
}

void EqpDOD::EvShowWindow()
{
	JKWindow::EvShowWindow();
	JKControl *win=GetControl(ID_EDIT+51);
	if(win) win->SetFocus();
}

void EqpDOD::EvButtonClick(WORD ctrlid)
{
	JKEdit *edit=(JKEdit *)GetControl(ID_EDIT+51);
	switch (ctrlid) {
		case ID_BUTTON+1:
			if(edit) edit->GetText(Buffer);
			HideWindow();
			SendControlMessage(EQPDODMESSAGE);
			break;
		case ID_BUTTON+2:
			if(edit) edit->SetText("");
			HideWindow();
			break;
	}
}

void EqpDOD::EvCharDown(uint16 key,WORD /*modifier*/)
{
	JKEdit *edit=(JKEdit *)GetControl(ID_EDIT+51);
	switch (key) {
		case ENTER:
			if(edit) edit->GetText(Buffer);
			HideWindow();
			SendControlMessage(EQPDODMESSAGE);
			break;
		case ESC:
			if(edit) edit->SetText("");
			HideWindow();
			break;
	}
}

void EqpDOD::EvEditFinish(WORD /*ctrlid*/)
{
	JKEdit *edit=(JKEdit *)GetControl(ID_EDIT+51);
	if(edit) edit->GetText(Buffer);
	HideWindow();
	SendControlMessage(EQPDODMESSAGE);
}

void EqpDOD::EvEditCancel(WORD /*ctrlid*/)
{
	JKEdit *edit=(JKEdit *)GetControl(ID_EDIT+51);
	if(edit) edit->SetText("");
	HideWindow();
}
