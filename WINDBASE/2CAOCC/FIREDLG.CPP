#include "jkbutton.h"
#include "typectrl.h"
#include "selctrl.h"
#include "firedlg.h"

FireDialog::FireDialog(FireRecord& rec):
	JKDialog(JKRect(210,250,820,500)),RefRecord(rec)
{
	WinAttr|=WA_TITLEMOVEABLE;
	SetText("¬a‰b ¶w ¥¡‹¡");
}

FireDialog::~FireDialog()
{
}

void FireDialog::SetupWindow()
{
	AddControl(new TypeEntryControl(  JKRect(150, 15,360, 35),ID_USERCONTROL+ 1,*RefRecord.GetEntry((uint16)0)));
	AddControl(new TypeEntryControl(  JKRect(150, 40,360, 60),ID_USERCONTROL+ 2,*RefRecord.GetEntry(1)));
	AddControl(new TypeEntryControl(  JKRect(150, 65,360, 85),ID_USERCONTROL+ 3,*RefRecord.GetEntry(2)));

	AddControl(new SelectEntryControl(JKRect(150, 90,360,110),ID_USERCONTROL+ 4,*RefRecord.GetEntry(3)));
	AddControl(new SelectEntryControl(JKRect(150,115,360,135),ID_USERCONTROL+ 5,*RefRecord.GetEntry(4)));

	AddControl(new TypeEntryControl(  JKRect(150,140,240,160),ID_USERCONTROL+ 6,*RefRecord.GetEntry(5)));
	AddControl(new TypeEntryControl(  JKRect(280,140,360,160),ID_USERCONTROL+ 7,*RefRecord.GetEntry(6)));
	AddControl(new TypeEntryControl(  JKRect(150,165,360,185),ID_USERCONTROL+ 8,*RefRecord.GetEntry(7)));
	AddControl(new TypeEntryControl(  JKRect(150,190,245,210),ID_USERCONTROL+ 9,*RefRecord.GetEntry(8)));
	AddControl(new TypeEntryControl(  JKRect(265,190,360,210),ID_USERCONTROL+10,*RefRecord.GetEntry(9)));

	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	JKButton *bttn;
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

void FireDialog::PaintClient(JKDC& dc,JKRect&)
{
	dc.PutIMG("log170.img",JKPoint(400,15));

	dc.IsOverWrite=FALSE;
	dc.TextOutX(JKRect(50, 15,145, 35),"¦”¡wÃ·",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(50, 40,145, 60),"Îa¸â¤åÑ¡",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(50, 65,145, 85),"¹ÁÎa",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(50, 90,145,110),"Îa¸â¬÷»©",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(50,115,145,135),"¶a wÒa‰Á",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(50,140,145,160),"Èe¹·",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(240,140,280,160),"¤i®",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(50,165,145,185),"¯¥‰Å",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(50,190,145,210),"¬a‰b·©¸a&¯¡ˆe",ADJ_RIGHT|ADJ_YCENTER);
}

void FireDialog::EvShowWindow()
{
	JKDialog::EvShowWindow();
	JKControl *ctrl=GetControl(ID_BUTTON+1);
	if(ctrl) ctrl->SetFocus();
}

void FireDialog::EvButtonClick(WORD ctrlid)
{
	if(ctrlid==ID_BUTTON+1)	SendMessage(ID_OK);
	else if(ctrlid==ID_BUTTON+2)	SendMessage(ID_CANCEL);
}

void FireDialog::EvDeadKeyDown(uint16 key,WORD /*modifier*/)
{
	WORD ctrlid;
	JKControl* ctrl;
	WORD LastCtrlID = ID_USERCONTROL+10;
	if(key==PGDNKEY){
		if(FindSubWindow(MsgFilter->FocusWinID,ctrlid,TRUE)&&ctrlid){
			if(ctrlid>=ID_USERCONTROL+1&&ctrlid<LastCtrlID) ctrlid++;
			else if(ctrlid==LastCtrlID) ctrlid=ID_BUTTON+1;
			else if(ctrlid==ID_BUTTON+1) ctrlid=ID_BUTTON+2;
			else if(ctrlid==ID_BUTTON+2) ctrlid=ID_USERCONTROL+1;
			else ctrlid=ID_USERCONTROL+1;

			ctrl=GetControl(ctrlid);
			if(ctrl) ctrl->SetFocus();
		}
	}
	else if(key==PGUPKEY){
		if(FindSubWindow(MsgFilter->FocusWinID,ctrlid,TRUE)&&ctrlid){
			if(ctrlid>ID_USERCONTROL+1&&ctrlid<=LastCtrlID) ctrlid--;
			else if(ctrlid==ID_USERCONTROL+1)	ctrlid=ID_BUTTON+2;
			else if(ctrlid==ID_BUTTON+1) ctrlid=LastCtrlID;
			else if(ctrlid==ID_BUTTON+2) ctrlid=ID_BUTTON+1;
			else ctrlid=ID_BUTTON+2;

			ctrl=GetControl(ctrlid);
			if(ctrl) ctrl->SetFocus();
		}
	}
}
