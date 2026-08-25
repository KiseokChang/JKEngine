#include "fltbttn.h"
#include "boolent.h"
#include "typectrl.h"
#include "selctrl.h"
#include "chkctrl.h"
#include "tgrefctl.h"
#include "hptdlg.h"


HPTDialog::HPTDialog(JKPoint p,HPTRecord& recbase,int16 currec):
	JKDialog(JKRect(0,0,250,360).OffsetBy(p),0,ID_SUBWINDOW+1),
	RecordViewBase(*this,recbase),
	CurrentIndex(currec)
{
	WinAttr&=~BYTE(WA_TITLEMOVEABLE);
	WinForm=0x03;
	BorderSize=0;
	BackColor=LtGray;
	SetText("Ð‚¯± Îa¸â ¶w");
}

HPTDialog::~HPTDialog()
{}

void HPTDialog::SetupWindow()
{
	AddControl(new TargetRefEntryControl(JKPoint(20,40),ID_USERCONTROL+1,(TargetRefEntry&)*RefRecord.GetEntry((uint16)0)));
	AddControl(new TypeEntryControl(JKRect(20,280,220,300),ID_USERCONTROL+2,*RefRecord.GetEntry(1)));
	AddControl(new CheckEntryControl(JKRect(20,305,50,325),ID_USERCONTROL+3,(BooleanEntry&)*RefRecord.GetEntry(2)));
}

void HPTDialog::PaintClient(JKDC& dc,JKRect&)
{
	dc.IsOverWrite=FALSE;
	dc.SetTextColor(LtBlue);
	dc.TextOutX(JKRect(20, 20,220, 40),"Îa¸â ¶w :",ADJ_YCENTER);
	dc.TextOutX(JKRect(20,260,220,280),"ÂA‹e Îa¸âˆ—¯¥¯¡ˆe",ADJ_XYCENTER);
	dc.TextOutX(JKRect(60,305,220,325),"‰¡ˆaÃ¡ µa¦",ADJ_YCENTER);
}

void HPTDialog::EvShowWindow()
{
	JKWindow::EvShowWindow();
	SetFocus();
}

ProcessResult HPTDialog::RespondMessage(JKMSG& msg)
{
	RV_RespondMessage(msg);
	return JKDialog::RespondMessage(msg);
}

void HPTDialog::EvDeadKeyDown(uint16 key,WORD /*modifier*/)
{
	WORD ctrlid;
	JKControl* ctrl;
	WORD LastCtrlID = ID_USERCONTROL+5;
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
