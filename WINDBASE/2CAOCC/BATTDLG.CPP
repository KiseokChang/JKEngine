#include "jkbutton.h"
#include "typectrl.h"
#include "selctrl.h"
#include "dandctrl.h"
#include "battdlg.h"

BattalionDialog::BattalionDialog(JKPoint p,BattalionRecord& rec,JKControl *parent):
	JKDialog(JKRect(0,0,600,310).OffsetBy(p),parent),RefRecord(rec)
{
	WinAttr|=WA_TITLEMOVEABLE;
  SetText("´aŠ… Í¡¥w ¦” ¸÷¥¡");
}

BattalionDialog::~BattalionDialog()
{
}

void BattalionDialog::SetupWindow()
{
	AddControl(new SelectEntryControl(JKRect(150, 15,360, 35),ID_USERCONTROL+ 1,*RefRecord.GetEntry((uint16)0)));
	AddControl(new TypeEntryControl(  JKRect(150, 40,360, 60),ID_USERCONTROL+ 2,*RefRecord.GetEntry( 1)));
	AddControl(new TypeEntryControl(  JKRect(150, 65,360, 85),ID_USERCONTROL+ 3,*RefRecord.GetEntry( 2)));
	AddControl(new TypeEntryControl(  JKRect(150, 90,360,110),ID_USERCONTROL+ 4,*RefRecord.GetEntry( 3)));
	AddControl(new SelectEntryControl(JKRect(150,115,360,135),ID_USERCONTROL+ 5,*RefRecord.GetEntry( 4)));
	AddControl(new TypeEntryControl(  JKRect(150,140,360,160),ID_USERCONTROL+ 6,*RefRecord.GetEntry( 5)));
	AddControl(new TypeEntryControl(  JKRect(150,165,245,185),ID_USERCONTROL+ 7,*RefRecord.GetEntry( 6)));
	AddControl(new TypeEntryControl(  JKRect(265,165,360,185),ID_USERCONTROL+ 8,*RefRecord.GetEntry( 7)));
	AddControl(new TypeEntryControl(  JKRect(150,190,245,210),ID_USERCONTROL+ 9,*RefRecord.GetEntry( 8)));
	AddControl(new TypeEntryControl(  JKRect(265,190,360,210),ID_USERCONTROL+10,*RefRecord.GetEntry( 9)));
	AddControl(new SelectEntryControl(JKRect(150,215,360,235),ID_USERCONTROL+11,*RefRecord.GetEntry(10)));
	AddControl(new DandaehoEntryControl(JKRect(150,240,360,270),ID_USERCONTROL+12,(DandaehoEntry& )*RefRecord.GetEntry(11)));
																																														 
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

void BattalionDialog::PaintClient(JKDC& dc,JKRect&)
{
	dc.PutIMG("log170.img",JKPoint(400,15));

	dc.IsOverWrite=FALSE;
	dc.TextOutX(JKRect(50, 15,145, 35),"¬w  ‹s  ¦  ”",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(50, 40,145, 60),"¦    ”    ¡w",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(50, 65,145, 85),"¤w µi ¤w ¶á ˆb",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(50, 90,145,110),"¦  ”  ¹Á  Îa",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(50,115,145,135),"Š          ‰w",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(50,140,145,160),"Í¡    ¢…    ®",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(50,165,145,185),"¥wb (Ñe/·¥ˆa)",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(50,190,145,210),"Èe´¢ (Ñe/·¥ˆa)",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(50,215,145,235),"¦  ”  ¬w  Ñ×",ADJ_RIGHT|ADJ_YCENTER);
  dc.TextOutX(JKRect(50,240,145,270),"”e    ”    Ñ¡",ADJ_RIGHT|ADJ_YCENTER);

	dc.TextOutX(JKRect(245,165,265,185),"/",ADJ_XYCENTER);
	dc.TextOutX(JKRect(245,190,265,210),"/",ADJ_XYCENTER);
}

void BattalionDialog::EvShowWindow()
{
	JKDialog::EvShowWindow();
	JKControl *ctrl=GetControl(ID_BUTTON+1);
	if(ctrl) ctrl->SetFocus();
}

void BattalionDialog::EvButtonClick(WORD ctrlid)
{
	if(ctrlid==ID_BUTTON+1)	SendMessage(ID_OK);
	else if(ctrlid==ID_BUTTON+2)	SendMessage(ID_CANCEL);
}

void BattalionDialog::EvDeadKeyDown(uint16 key,WORD /*modifier*/)
{
	WORD ctrlid;
	JKControl* ctrl;
	WORD LastCtrlID = ID_USERCONTROL+11;
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
