#include "jkbutton.h"
#include "okdialog.h"

OKDialog::OKDialog(JKRect rect,int8 *msgstr,JKControl *parent):
	JKDialog(rect,parent),MessageString(0)
{
	WORD len=msgstr?strlen(msgstr):0;
	MessageString = new BYTE[len+1];
	if(msgstr) {
		strcpy(MessageString,msgstr);
		MessageString[len]=0;
	}
	else MessageString[0]=0;
}

OKDialog::~OKDialog()
{
	if(MessageString) delete[] MessageString;
}

void OKDialog::SetupWindow()
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.Expand(-10);

	rect.bottom-=50;
	JKStatic *stc=new JKStatic(rect,ID_STATIC+1);
	stc->BackColor=BackColor;
	stc->SetText(MessageString);
	stc->AdjustFlag=ADJ_XYCENTER;
	AddControl(stc);

	int16 width=rect.Width();
	rect=JKRect(width/2-30,rect.bottom+20,width/2+30,rect.bottom+40);
	JKButton *bttn=new JKButton(rect,ID_BUTTON+1);
	bttn->SetText("ÑÂ·¥");
	AddControl(bttn);
}

void OKDialog::EvShowWindow()
{
	JKDialog::EvShowWindow();
	JKControl *ctrl=GetControl(ID_BUTTON+1);
	if(ctrl) ctrl->SetFocus();
}

void OKDialog::EvButtonClick(WORD ctrlid)
{
	if(ctrlid==ID_BUTTON+1){
		SendMessage(ID_OK);
	}
}

void OKDialog::EvCharDown(WORD key,WORD)
{
	if(key==ENTER){
		SendMessage(ID_OK);
	}
}
