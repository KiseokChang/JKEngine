#include "jkbutton.h"
#include "typectrl.h"
#include "selctrl.h"
#include "dandctrl.h"
#include "xbattdlg.h"

XBattalionDialog::XBattalionDialog(JKPoint p,XBattalionRecord& rec,JKControl *parent):
	JKDialog(JKRect(0,0,620,240).OffsetBy(p),parent),RefRecord(rec)
{
	WinAttr|=WA_TITLEMOVEABLE;
  SetText("´aŠ… ‹¡Èa ¦” ¸÷¥¡ ·³b");
}

XBattalionDialog::~XBattalionDialog()
{
}

void XBattalionDialog::SetupWindow()
{
	AddControl(new SelectEntryControl(JKRect(150, 15,360, 35),ID_USERCONTROL+1,*RefRecord.GetEntry((uint16)0)));
	AddControl(new TypeEntryControl(  JKRect(150, 40,360, 60),ID_USERCONTROL+2,*RefRecord.GetEntry( 1)));
	AddControl(new TypeEntryControl(  JKRect(150, 65,360, 85),ID_USERCONTROL+3,*RefRecord.GetEntry( 2)));
	AddControl(new TypeEntryControl(  JKRect(150, 90,360,110),ID_USERCONTROL+4,*RefRecord.GetEntry( 3)));
	AddControl(new TypeEntryControl(  JKRect(150,115,340,135),ID_USERCONTROL+5,*RefRecord.GetEntry( 4)));
	AddControl(new DandaehoEntryControl(JKRect(150,140,360,170),ID_USERCONTROL+6,(DandaehoEntry& )*RefRecord.GetEntry(5)));

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

void XBattalionDialog::PaintClient(JKDC& dc,JKRect&)
{
	dc.PutIMG("log170.img",JKPoint(400,10));

	dc.IsOverWrite=FALSE;
	dc.TextOutX(JKRect(50, 15,145, 35),"¬w  ‹s  ¦  ”",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(50, 40,145, 60),"¦    ”    ¡w",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(50, 65,145, 85),"¦  ”  ¹Á  Îa",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(50, 90,145,110),"§¡          ‰¡",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(50,115,145,135),"¸å    Ê    b",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(340,115,360,135),"%",ADJ_XYCENTER);
	dc.TextOutX(JKRect(50,140,145,170),"”e    ”    Ñ¡",ADJ_RIGHT|ADJ_YCENTER);
}

void XBattalionDialog::EvShowWindow()
{
	JKDialog::EvShowWindow();
	JKControl *ctrl=GetControl(ID_BUTTON+1);
	if(ctrl) ctrl->SetFocus();
}

void XBattalionDialog::EvButtonClick(WORD ctrlid)
{
	if(ctrlid==ID_BUTTON+1)	SendMessage(ID_OK);
	else if(ctrlid==ID_BUTTON+2)	SendMessage(ID_CANCEL);
}

void XBattalionDialog::EvDeadKeyDown(uint16 key,WORD /*modifier*/)
{
	WORD ctrlid;
	JKControl* ctrl;
	WORD LastCtrlID = ID_USERCONTROL+6;
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

