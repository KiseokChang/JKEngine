#include "jkbutton.h"
#include "sel1of2.h"

SelectOneOfTwoDialog::SelectOneOfTwoDialog(JKPoint p,char *text1,char *text2,JKControl *parent):
	JKDialog(JKRect(0,0,260,120).OffsetBy(p),parent)
{
	if(text1){ strncpy(Text1,text1,100); Text1[100]=0;}
	else Text1[0]=0;
	if(text2){ strncpy(Text2,text2,100); Text2[100]=0;}
	else Text2[0]=0;
  WinAttr|=WA_TITLEMOVEABLE;
}

SelectOneOfTwoDialog::~SelectOneOfTwoDialog()
{
}

void SelectOneOfTwoDialog::SetupWindow()
{
	JKButton *bttn;

	bttn=new JKButton(JKRect(10,10,100,40),ID_BUTTON+2);
	bttn->SetText(Text1);
	AddControl(bttn);
	bttn=new JKButton(JKRect(10,50,100,80),ID_BUTTON+3);
	bttn->SetText(Text2);
	AddControl(bttn);

	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.top=rect.bottom-30;
	rect.left=rect.right-100;
	bttn=new JKButton(rect,ID_BUTTON+1);
	bttn->SetText("Âá­¡");
	AddControl(bttn);
}

void SelectOneOfTwoDialog::EvShowWindow()
{
	JKDialog::EvShowWindow();
	JKControl *ctrl=GetControl(ID_BUTTON+2);
	if(ctrl) ctrl->SetFocus();
}

void SelectOneOfTwoDialog::EvButtonClick(WORD ctrlid)
{
	if(ctrlid==ID_BUTTON+1) {
		SendMessage(ID_CANCEL);
	}
	else{
		if(ctrlid==ID_BUTTON+2) ReturnValue=1;
		else if(ctrlid==ID_BUTTON+3) ReturnValue=2;
		CloseWindow();
	}
}

void SelectOneOfTwoDialog::EvDeadKeyDown(uint16 key,WORD /*modifier*/)
{
	WORD ctrlid;
	JKControl* ctrl;
	WORD LastCtrlID = ID_BUTTON+3;
	if(key==PGDNKEY){
		if(FindSubWindow(MsgFilter->FocusWinID,ctrlid,TRUE)&&ctrlid){
			if(ctrlid>=ID_BUTTON+1&&ctrlid<LastCtrlID) ctrlid++;
			else if(ctrlid==LastCtrlID) ctrlid=ID_BUTTON+1;
			else ctrlid=ID_BUTTON+1;

			ctrl=GetControl(ctrlid);
			if(ctrl) ctrl->SetFocus();
		}
	}
	else if(key==PGUPKEY){
		if(FindSubWindow(MsgFilter->FocusWinID,ctrlid,TRUE)&&ctrlid){
			if(ctrlid>ID_BUTTON+1&&ctrlid<=LastCtrlID) ctrlid--;
			else if(ctrlid==ID_BUTTON+1)	ctrlid=LastCtrlID;
			else ctrlid=LastCtrlID;

			ctrl=GetControl(ctrlid);
			if(ctrl) ctrl->SetFocus();
		}
	}
}
