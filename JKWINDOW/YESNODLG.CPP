#include <ctype.h>
#include "jkbutton.h"
#include "yesnodlg.h"

YesNoDialog::YesNoDialog(JKRect rect,int8 *msgstr,JKControl *parent):
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

YesNoDialog::~YesNoDialog()
{
	if(MessageString) delete[] MessageString;
}

void YesNoDialog::SetupWindow()
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.Expand(-10);

	rect.bottom-=50;
	JKStatic *stc=new JKStatic(rect,ID_STATIC+1);
	stc->BackColor=BackColor;
	stc->SetText(MessageString);
	stc->AdjustFlag=ADJ_XYCENTER;
	AddControl(stc);

	rect.top=rect.bottom+20;
	rect.bottom=rect.top+20;
	int16 width =rect.Width();
	JKRect bttnrect=JKRect(rect.left+width/6,rect.top,rect.left+width/6+60,rect.bottom);
	JKButton *bttn=new JKButton(bttnrect,ID_BUTTON+1);
	bttn->SetText("µ");
	AddControl(bttn);

	bttnrect=JKRect(rect.right-width/6-60,rect.top,rect.right-width/6,rect.bottom);
	bttn=new JKButton(bttnrect,ID_BUTTON+2);
	bttn->SetText("´a“¡µ¡");
	AddControl(bttn);

}

void YesNoDialog::EvShowWindow()
{
	JKDialog::EvShowWindow();
	JKControl *ctrl=GetControl(ID_BUTTON+1);
	if(ctrl) ctrl->SetFocus();
}

void YesNoDialog::EvDeadCharDown(WORD key,WORD /*modifier*/)
{
	if(key=='\t'){
		WORD ctrlid;
		FindSubWindow(MsgFilter->FocusWinID,ctrlid,TRUE);
		if(ctrlid){
			JKControl* ctrl;
			if(ctrlid==ID_BUTTON+1) ctrlid=ID_BUTTON+2;
			else ctrlid=ID_BUTTON+1;
			ctrl=GetControl(ctrlid);
			if(ctrl) ctrl->SetFocus();
		}
	}
}

void YesNoDialog::EvButtonClick(WORD ctrlid)
{
	if(ctrlid==ID_BUTTON+1){
		SendMessage(ID_YES);
	}
	else if(ctrlid==ID_BUTTON+2){
		SendMessage(ID_NO);
	}
}

void YesNoDialog::EvCharDown(WORD key,WORD)
{
	if(key==ESC){
		SendMessage(ID_CANCEL);
	}
	else{
		key=toupper(key);
		if(key=='Y') SendMessage(ID_YES);
		else if(key=='N') SendMessage(ID_NO);
	}
}
