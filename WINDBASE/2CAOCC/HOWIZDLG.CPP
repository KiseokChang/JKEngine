#include "jkbutton.h"
#include "listbox.h"
#include "typectrl.h"
#include "selctrl.h"
#include "howizman.h"
#include "howizdlg.h"

HowitzerDialog::HowitzerDialog(JKRect rect,HowitzerRecord& rec,JKControl *parent):
	JKDialog(rect,parent,ID_SUBWINDOW+1),RecordViewBase(*this,rec)
{
	WinForm=0x01;
}

HowitzerDialog::~HowitzerDialog()
{
}

void HowitzerDialog::SetupWindow()
{
	AddControl(new SelectEntryControl(JKRect(200,10,430,30),ID_USERCONTROL+1,*RefRecord.GetEntry((uint16)0)));
	AddControl(new TypeEntryControl(  JKRect(200,35,310,55),ID_USERCONTROL+2,*RefRecord.GetEntry( 1)));
	AddControl(new TypeEntryControl(  JKRect(320,35,430,55),ID_USERCONTROL+3,*RefRecord.GetEntry( 2)));
	AddControl(new TypeEntryControl(  JKRect(200,60,310,80),ID_USERCONTROL+4,*RefRecord.GetEntry( 3)));
	AddControl(new TypeEntryControl(  JKRect(320,60,430,80),ID_USERCONTROL+5,*RefRecord.GetEntry( 4)));
	AddControl(new TypeEntryControl(  JKRect(200,85,310,105),ID_USERCONTROL+6,*RefRecord.GetEntry( 5)));
	AddControl(new TypeEntryControl(  JKRect(320,85,430,105),ID_USERCONTROL+7,*RefRecord.GetEntry( 6)));
	AddControl(new TypeEntryControl(  JKRect(200,110,310,130),ID_USERCONTROL+8,*RefRecord.GetEntry( 7)));
	AddControl(new TypeEntryControl(  JKRect(320,110,430,130),ID_USERCONTROL+9,*RefRecord.GetEntry( 8)));
}

void HowitzerDialog::PaintClient(JKDC& dc,JKRect&)
{
	dc.IsOverWrite=FALSE;
	dc.TextOutX(JKRect(20, 10,195, 30),"Š               ‰w :",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(20, 35,195, 55),"¬aˆáŸ¡(¥¡É·Èe/ RAP) :",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(20, 60,195, 80),"¸å·¡Ðe‰(¹Á  /  ¶) :",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(20, 85,195,105),"¤i¬a­¢•¡(ÂA”/»¡­¢) :",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(20,110,195,130),"1 ¤iÒa‰Á(ì·¡/¹·¯±) :",ADJ_RIGHT|ADJ_YCENTER);
}

ProcessResult HowitzerDialog::RespondMessage(JKMSG& msg)
{
	RV_RespondMessage(msg);
	return JKDialog::RespondMessage(msg);
}

void HowitzerDialog::EvShowWindow()
{
	JKDialog::EvShowWindow();
	JKControl *ctrl=GetControl(ID_BUTTON+1);
	if(ctrl) ctrl->SetFocus();
}

void HowitzerDialog::EvDeadKeyDown(uint16 key,WORD /*modifier*/)
{
	WORD ctrlid;
	JKControl* ctrl;
	WORD LastCtrlID = ID_USERCONTROL+9;
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
