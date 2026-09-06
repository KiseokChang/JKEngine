#include "jkbutton.h"
#include "typectrl.h"
#include "selctrl.h"
#include "tgrefctl.h"
#include "plntgdlg.h"

PlanTargetDialog::PlanTargetDialog(PlanTargetRecord& rec):
	JKDialog(JKRect(240,100,800,600)),RefRecord(rec)
{
	WinAttr|=WA_TITLEMOVEABLE;
	SetText("‰ÒB Îa¸â ·³b");
}

PlanTargetDialog::~PlanTargetDialog()
{
}
void PlanTargetDialog::SetupWindow()
{
	JKControl *ctrl=new TargetRefEntryControl(JKPoint(20,10),ID_USERCONTROL+1,(TargetRefEntry&)*RefRecord.GetEntry((uint16)0));
	AddControl(ctrl);
	AddControl(new SelectEntryControl(JKRect(205,240,315,260),ID_USERCONTROL+2,*RefRecord.GetEntry(1)));

///////////////////////////////////////////////////////////////////////////////

	for(uint16 i=0;i<5;i++){
		AddControl(new TypeEntryControl(JKRect( 35,315+i*25,115,335+i*25),ID_USERCONTROL+3+i*5,*RefRecord.GetEntry(2+i*5)));
		AddControl(new TypeEntryControl(JKRect(135,315+i*25,215,335+i*25),ID_USERCONTROL+4+i*5,*RefRecord.GetEntry(3+i*5)));
		AddControl(new TypeEntryControl(JKRect(235,315+i*25,315,335+i*25),ID_USERCONTROL+5+i*5,*RefRecord.GetEntry(4+i*5)));
		AddControl(new TypeEntryControl(JKRect(355,315+i*25,415,335+i*25),ID_USERCONTROL+6+i*5,*RefRecord.GetEntry(5+i*5)));
		AddControl(new TypeEntryControl(JKRect(455,315+i*25,515,335+i*25),ID_USERCONTROL+7+i*5,*RefRecord.GetEntry(6+i*5)));
	}

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

void PlanTargetDialog::PaintClient(JKDC& dc,JKRect&)
{
	dc.PutIMG("log170.img",JKPoint(370,5));

	dc.IsOverWrite=FALSE;
	dc.TextOutX(JKRect(20,240,100,260),"‰·‰b¤w¤ó",ADJ_RIGHT|ADJ_YCENTER);

	dc.TextOutX(JKRect( 50,290,115,310),"¦”¡w",ADJ_XYCENTER);
	dc.TextOutX(JKRect(150,290,215,310),"Èe¹·",ADJ_XYCENTER);
	dc.TextOutX(JKRect(250,290,315,310),"¤i®",ADJ_XYCENTER);
	dc.TextOutX(JKRect(350,290,415,310),"¬a‰b¯¡¸b¯¡ˆb",ADJ_XYCENTER);
	dc.TextOutX(JKRect(450,290,515,310),"¬a‰b¹·ža¯¡ˆb",ADJ_XYCENTER);

	for(uint16 i=0;i<5;i++){
		dc.TextOutX(JKRect(335,315+i*25,345,335+i*25),"H",ADJ_XYCENTER);
		dc.TextOutX(JKRect(415,315+i*25,435,335+i*25),"~",ADJ_XYCENTER);
		dc.TextOutX(JKRect(435,315+i*25,445,335+i*25),"H",ADJ_XYCENTER);
	}
}

void PlanTargetDialog::EvShowWindow()
{
	JKDialog::EvShowWindow();
	JKControl *ctrl=GetControl(ID_USERCONTROL+1);
	if(ctrl) ctrl->SetFocus();
}

void PlanTargetDialog::EvButtonClick(WORD ctrlid)
{
	if(ctrlid==ID_BUTTON+1)	SendMessage(ID_OK);
	else if(ctrlid==ID_BUTTON+2)	SendMessage(ID_CANCEL);
}

void PlanTargetDialog::EvDeadKeyDown(uint16 key,WORD /*modifier*/)
{
	WORD ctrlid;
	JKControl* ctrl;
  WORD LastCtrlID = ID_USERCONTROL+27;
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
