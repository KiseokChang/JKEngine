#include "jkbutton.h"
#include "jkedit.h"
#include "okdialog.h"
#include "passdlg.h"

PasswordDialog::PasswordDialog(JKRect rect,char *passname,char *msg,JKControl *parent):
	JKDialog(rect,parent),Password(passname),MessageString(msg),PasswordLength(8)
{
	WinForm=0x01;
	BackColor=LtRed;
	if(!Password) EventHandler::CreationError=JKControl::CreationError=TRUE;
}

void PasswordDialog::SetupWindow()
{
	JKStatic *stc;
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.left+=20;
	rect.right-=20;
	rect.top=20;
	rect.bottom-=90;
	if(MessageString){
		stc=new JKStatic(rect,ID_STATIC+1);
		stc->BackColor=BackColor;
		stc->SetText(MessageString);
		stc->AdjustFlag=ADJ_XYCENTER;
		AddControl(stc);
	}

	rect.top=rect.bottom+15;
	rect.bottom=rect.top+19;
	rect.left+=20;
	rect.right-=20;
	JKEdit *edit= new JKEdit(rect,ID_EDIT+1,0,PasswordLength,FALSE,FALSE);
	edit->IsPassword=TRUE;
	AddControl(edit);

	rect.top=rect.bottom+15;
	rect.bottom=rect.top+25;

	stc= new JKButton(JKRect(rect.left,rect.top,rect.left+80,rect.bottom),ID_BUTTON+1);
	stc->SetText("ÑÂ ·¥");
	AddControl(stc);

	stc= new JKButton(JKRect(rect.right-80,rect.top,rect.right,rect.bottom),ID_BUTTON+2);
	stc->SetText("Âá ­¡");
	AddControl(stc);
}

void PasswordDialog::EvShowWindow()
{
	JKDialog::EvShowWindow();
	JKControl *ctrl=GetControl(ID_EDIT+1);
	if(ctrl) ctrl->SetFocus();
}


BOOL PasswordDialog::GetPassword(char *buffer)
{
	if(buffer){
		JKControl *ctrl=GetControl(ID_EDIT+1);
		if(ctrl){
			char *str=ctrl->GetText();
			if(str){
				strncpy(buffer,str,8);
				if(strlen(str)<8) memset(buffer+strlen(str),' ',8-strlen(str));
				buffer[8]=0;
				return TRUE;
			}
		}
	}
	return FALSE;
}

void PasswordDialog::EvButtonClick(WORD ctrlid)
{
	JKControl *ctrl;
	switch(ctrlid){
		case ID_BUTTON+1:
			BYTE Buffer[9];
			if(GetPassword(Buffer)){
				if(!strcmp(Password,Buffer)){
					OKDialog okdlg(GetWindowRectOnScreen(),"µ°·e ´qÑ¡ˆa ·³b–A´ö¯s“¡”a.");
					okdlg.WinForm&=0x03;
					okdlg.SetText("´qÑ¡ ·³b ‰i‰Á");
					okdlg.Run();
					SendMessage(ID_OK);
				}
				else {
					OKDialog okdlg(GetWindowRectOnScreen(),"¸i¡µ–E ´qÑ¡ˆa ·³b–A´ö¯s“¡”a.");
					okdlg.WinForm&=0x03;
					okdlg.SetText("´qÑ¡ ·³b ‰i‰Á");
					okdlg.Run();
					SendMessage(ID_CANCEL);
				}
			}
			break;
		case ID_BUTTON+2:
			SendMessage(ID_CANCEL);
			break;
		default:
			break;
	}
}


void PasswordDialog::EvDeadKeyDown(uint16 key,WORD /*modifier*/)
{
	WORD ctrlid;
	JKControl* ctrl;
	WORD LastCtrlID = ID_EDIT+1;
	if(key==PGDNKEY){
		if(FindSubWindow(MsgFilter->FocusWinID,ctrlid,TRUE)&&ctrlid){
			if(ctrlid==ID_EDIT+1) ctrlid=ID_BUTTON+1;
			else if(ctrlid==ID_BUTTON+1) ctrlid=ID_BUTTON+2;
			else if(ctrlid==ID_BUTTON+2) ctrlid=ID_EDIT+1;
			else ctrlid=ID_EDIT+1;

			ctrl=GetControl(ctrlid);
			if(ctrl) ctrl->SetFocus();
		}
	}
	else if(key==PGUPKEY){
		if(FindSubWindow(MsgFilter->FocusWinID,ctrlid,TRUE)&&ctrlid){
			if(ctrlid==ID_EDIT+1)	ctrlid=ID_BUTTON+2;
			else if(ctrlid==ID_BUTTON+1) ctrlid=LastCtrlID;
			else if(ctrlid==ID_BUTTON+2) ctrlid=ID_BUTTON+1;
			else ctrlid=ID_BUTTON+2;

			ctrl=GetControl(ctrlid);
			if(ctrl) ctrl->SetFocus();
		}
	}
}
