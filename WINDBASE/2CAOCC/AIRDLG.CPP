#include "fltbttn.h"
#include "boolent.h"
#include "typectrl.h"
#include "selctrl.h"
#include "airdlg.h"


AirDialog::AirDialog(JKRect rect,AirRecord& recbase,JKControl *parent):
	JKDialog(rect,parent,ID_SUBWINDOW+1),RecordViewBase(*this,recbase)
{
	WinAttr&=~BYTE(WA_TITLEMOVEABLE);
	WinForm=0x03;
	BorderSize=0;
	BackColor=LtGray;
	SetText("‰·µb É·¹A ¶w");
}

AirDialog::~AirDialog()
{}

void AirDialog::SetupWindow()
{
	JKRect rect(0,40,0,60);
	rect.left=rect.right+20; rect.right=rect.left+100;
	AddControl(new TypeEntryControl(rect,ID_USERCONTROL+1,*RefRecord.GetEntry((uint16)0)));

	rect.left=rect.right+20; rect.right=rect.left+100;
	AddControl(new TypeEntryControl(rect,ID_USERCONTROL+2,*RefRecord.GetEntry(1)));

	rect.left=rect.right+20; rect.right=rect.left+100;
	AddControl(new TypeEntryControl(rect,ID_USERCONTROL+3,*RefRecord.GetEntry(2)));

	rect.left=rect.right+20; rect.right=rect.left+100;
	AddControl(new TypeEntryControl(rect,ID_USERCONTROL+4,*RefRecord.GetEntry(3)));

	rect.Set(0,80,0,100);
	rect.left=rect.right+20; rect.right=rect.left+60;
	AddControl(new TypeEntryControl(rect,ID_USERCONTROL+5,*RefRecord.GetEntry(4)));
	rect.left=rect.right+10; rect.right=rect.left+60;
	AddControl(new TypeEntryControl(rect,ID_USERCONTROL+6,*RefRecord.GetEntry(5)));

	rect.left=rect.right+10; rect.right=rect.left+20;
	AddControl(new TypeEntryControl(rect,ID_USERCONTROL+7,*RefRecord.GetEntry(6)));
	rect.left=rect.right+10; rect.right=rect.left+100;
	AddControl(new TypeEntryControl(rect,ID_USERCONTROL+8,*RefRecord.GetEntry(7)));
	rect.left=rect.right+10; rect.right=rect.left+20;
	AddControl(new TypeEntryControl(rect,ID_USERCONTROL+9,*RefRecord.GetEntry(8)));
	rect.left=rect.right+10; rect.right=rect.left+100;
	AddControl(new TypeEntryControl(rect,ID_USERCONTROL+10,*RefRecord.GetEntry(9)));
}

void AirDialog::PaintClient(JKDC& dc,JKRect&)
{
	dc.IsOverWrite=FALSE;
	dc.SetTextColor(LtBlue);

	JKRect rect(0,20,0,40);
	rect.left=rect.right+20; rect.right=rect.left+100;
	dc.TextOutX(rect,"¹Á   Îa 1",ADJ_XYCENTER);
	rect.left=rect.right+20; rect.right=rect.left+100;
	dc.TextOutX(rect,"¹Á   Îa 2",ADJ_XYCENTER);
	rect.left=rect.right+20; rect.right=rect.left+100;
	dc.TextOutX(rect,"¹Á   Îa 3",ADJ_XYCENTER);
	rect.left=rect.right+20; rect.right=rect.left+100;
	dc.TextOutX(rect,"¹Á   Îa 4",ADJ_XYCENTER);
	rect.Set(0,60,0,80);
	rect.left=rect.right+20; rect.right=rect.left+60;
	dc.TextOutX(rect,"ÂA¸á‰¡•¡",ADJ_XYCENTER);
	rect.left=rect.right+10; rect.right=rect.left+60;
	dc.TextOutX(rect,"ÂA‰¡‰¡•¡",ADJ_XYCENTER);
	rect.left=rect.right+10; rect.right=rect.left+130;
	dc.TextOutX(rect,"¸â¶w¯¡¸b¯¡ˆe",ADJ_XYCENTER);
	rect.left=rect.right+10; rect.right=rect.left+130;
	dc.TextOutX(rect,"¸â¶w{¯¡ˆe",ADJ_XYCENTER);
}

ProcessResult AirDialog::RespondMessage(JKMSG& msg)
{
	RV_RespondMessage(msg);
	return JKDialog::RespondMessage(msg);
}

void AirDialog::EvShowWindow()
{
	JKWindow::EvShowWindow();
	SetFocus();
}

void AirDialog::EvDeadKeyDown(uint16 key,WORD /*modifier*/)
{
	WORD ctrlid;
	JKControl* ctrl;
	WORD LastCtrlID = ID_USERCONTROL+10;
	if(key==PGDNKEY){
		if(FindSubWindow(MsgFilter->FocusWinID,ctrlid,TRUE)&&ctrlid){
			if(ctrlid>=ID_USERCONTROL+1&&ctrlid<LastCtrlID) ctrlid++;
			else if(ctrlid==LastCtrlID) ctrlid=ID_USERCONTROL+1;
			else ctrlid=ID_USERCONTROL+1;

			ctrl=GetControl(ctrlid);
			if(ctrl) ctrl->SetFocus();
		}
	}
	else if(key==PGUPKEY){
		if(FindSubWindow(MsgFilter->FocusWinID,ctrlid,TRUE)&&ctrlid){
			if(ctrlid>ID_USERCONTROL+1&&ctrlid<=LastCtrlID) ctrlid--;
			else if(ctrlid==ID_USERCONTROL+1)	ctrlid=LastCtrlID;
			else ctrlid=LastCtrlID;

			ctrl=GetControl(ctrlid);
			if(ctrl) ctrl->SetFocus();
		}
	}
}
