#include "jkbutton.h"
#include "typectrl.h"
#include "getwidth.h"

GetWidthDialog::GetWidthDialog(JKPoint p,int32& width,JKControl *parent):
	JKDialog(JKRect(0,0,360,120).OffsetBy(p),parent),
	RefWidth(width),NumEnt(0,"",0,10000,10,RefWidth)
{
	WinAttr|=WA_TITLEMOVEABLE;
  SetText("¬å ŠŠ‹¡ »¡¸÷");
}

GetWidthDialog::~GetWidthDialog()
{
}

void GetWidthDialog::SetupWindow()
{
	AddControl(new TypeEntryControl(JKRect(100, 20,310, 40),ID_USERCONTROL+ 1,NumEnt));

	JKButton *bttn;
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.top=rect.bottom-30;
	rect.left=rect.right-100;
	bttn=new JKButton(rect,ID_BUTTON+2);
	bttn->SetText("Âá­¡");
	AddControl(bttn);

	rect.right=rect.left-10;
	rect.left=rect.right-100;
	bttn=new JKButton(rect,ID_BUTTON+1);
	bttn->SetText("ÑÂ·¥");
	AddControl(bttn);
}

void GetWidthDialog::PaintClient(JKDC& dc,JKRect&)
{
	dc.IsOverWrite=FALSE;
	dc.TextOutX(JKRect(50, 20,95, 40),"",ADJ_RIGHT|ADJ_YCENTER);
}

void GetWidthDialog::EvShowWindow()
{
	JKDialog::EvShowWindow();
	JKControl *ctrl=GetControl(ID_USERCONTROL+1);
	if(ctrl) ctrl->SetFocus();
}

void GetWidthDialog::EvButtonClick(WORD ctrlid)
{
	if(ctrlid==ID_BUTTON+1) {
		int32 num;
		NumEnt.GetNumber(num);
		if(num>=0) RefWidth=num;
		SendMessage(ID_OK);
	}
	else if(ctrlid==ID_BUTTON+2) SendMessage(ID_CANCEL);
}

void GetWidthDialog::EvDeadKeyDown(uint16 key,WORD /*modifier*/)
{
	WORD ctrlid;
	JKControl* ctrl;
	WORD LastCtrlID = ID_USERCONTROL+1;
	if(key==PGDNKEY){
		if(FindSubWindow(MsgFilter->FocusWinID,ctrlid,TRUE)&&ctrlid){
			if(ctrlid==ID_USERCONTROL+1) ctrlid=ID_BUTTON+1;
			else if(ctrlid==ID_BUTTON+1) ctrlid=ID_BUTTON+2;
			else if(ctrlid==ID_BUTTON+2) ctrlid=ID_USERCONTROL+1;
			else ctrlid=ID_USERCONTROL+1;

			ctrl=GetControl(ctrlid);
			if(ctrl) ctrl->SetFocus();
		}
	}
	else if(key==PGUPKEY){
		if(FindSubWindow(MsgFilter->FocusWinID,ctrlid,TRUE)&&ctrlid){
			if(ctrlid==ID_USERCONTROL+1)	ctrlid=ID_BUTTON+2;
			else if(ctrlid==ID_BUTTON+1) ctrlid=LastCtrlID;
			else if(ctrlid==ID_BUTTON+2) ctrlid=ID_BUTTON+1;
			else ctrlid=ID_BUTTON+2;

			ctrl=GetControl(ctrlid);
			if(ctrl) ctrl->SetFocus();
		}
	}
}
