#include "jkbutton.h"
#include "typectrl.h"
#include "selctrl.h"
#include "chkctrl.h"
#include "dandctrl.h"
#include "tgdlg.h"

TargetDialog::TargetDialog(JKPoint p,TargetRecord& rec,JKControl *parent):
	JKDialog(JKRect(0,0,550,230).OffsetBy(p),parent),RefRecord(rec)
{
	WinAttr|=WA_TITLEMOVEABLE;
	SetText("Îa¸â  ¶w");
}

TargetDialog::~TargetDialog()
{
}

void TargetDialog::SetupWindow()
{
	AddControl(new TypeEntryControl(  JKRect(105, 15,315, 35),ID_USERCONTROL+ 1,*RefRecord.GetEntry((uint16)0)));
	AddControl(new TypeEntryControl(  JKRect(105, 40,315, 60),ID_USERCONTROL+ 2,*RefRecord.GetEntry(1)));
	AddControl(new SelectEntryControl(JKRect(105, 65,315, 85),ID_USERCONTROL+ 3,*RefRecord.GetEntry(2)));
	AddControl(new TypeEntryControl(  JKRect(105, 90,315,110),ID_USERCONTROL+ 4,*RefRecord.GetEntry(3)));
	AddControl(new TypeEntryControl(  JKRect(105,115,315,135),ID_USERCONTROL+ 5,*RefRecord.GetEntry(4)));
	AddControl(new TypeEntryControl( JKRect(105,140,315,160),ID_USERCONTROL+ 6,*RefRecord.GetEntry(5)));
	AddControl(new DandaehoEntryControl(JKRect(105,165,315,195),ID_USERCONTROL+ 7,(DandaehoEntry&)*RefRecord.GetEntry(6)));

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

void TargetDialog::PaintClient(JKDC& dc,JKRect&)
{
	dc.PutIMG("log170.img",JKPoint(350,10));

	dc.IsOverWrite=FALSE;
	dc.TextOutX(JKRect(20, 15,100, 35),"Îa ¸â  ¤å Ñ¡",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(20, 40,100, 60),"Îa ¸â  ¹Á Îa",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(20, 65,100, 85),"Îa ¸â  ¬÷ »©",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(20, 90,100,110),"Îa        ‰¡",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(20,115,100,135),"§¡        ‰¡",ADJ_RIGHT|ADJ_YCENTER);
  dc.TextOutX(JKRect(20,140,100,160),"¸å   Ê   b",ADJ_RIGHT|ADJ_YCENTER);
  dc.TextOutX(JKRect(20,165,100,195),"”e   ”   Ñ¡",ADJ_RIGHT|ADJ_YCENTER);
}


void TargetDialog::EvShowWindow()
{
	JKDialog::EvShowWindow();
	JKControl *ctrl=GetControl(ID_BUTTON+1);
	if(ctrl) ctrl->SetFocus();
}

void TargetDialog::EvButtonClick(WORD ctrlid)
{
	if(ctrlid==ID_BUTTON+1)	SendMessage(ID_OK);
	else if(ctrlid==ID_BUTTON+2)	SendMessage(ID_CANCEL);
}

void TargetDialog::EvDeadKeyDown(uint16 key,WORD /*modifier*/)
{
	WORD ctrlid;
	JKControl* ctrl;
	WORD LastCtrlID = ID_USERCONTROL+4;
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
