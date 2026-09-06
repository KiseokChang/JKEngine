#include "jkbutton.h"
#include "typectrl.h"
#include "selctrl.h"
#include "hopedlg.h"

YomangAcquisitionDialog::YomangAcquisitionDialog(FireRecord& firerec):
	JKDialog(JKRect(340,200,700,320)),FireRec(firerec)
{
	WinAttr|=WA_TITLEMOVEABLE;
	SetText("¶a w Òa‰Á ·³b");
}

YomangAcquisitionDialog::~YomangAcquisitionDialog()
{
}

void YomangAcquisitionDialog::SetupWindow()
{
	AddControl(new SelectEntryControl(JKRect(100,20,310,40),ID_USERCONTROL+ 1,*FireRec.GetEntry(4)));

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

void YomangAcquisitionDialog::PaintClient(JKDC& dc,JKRect&)
{
	dc.IsOverWrite=FALSE;
	dc.TextOutX(JKRect(50,20,95,40),"¶a wÒa‰Á",ADJ_RIGHT|ADJ_YCENTER);
}

void YomangAcquisitionDialog::EvShowWindow()
{
	JKDialog::EvShowWindow();
	JKControl *ctrl=GetControl(ID_USERCONTROL+1);
	if(ctrl) ctrl->SetFocus();
}

void YomangAcquisitionDialog::EvButtonClick(WORD ctrlid)
{
	if(ctrlid==ID_BUTTON+1) SendMessage(ID_OK);
	else if(ctrlid==ID_BUTTON+2) SendMessage(ID_CANCEL);
}

void YomangAcquisitionDialog::EvDeadKeyDown(uint16 key,WORD /*modifier*/)
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
