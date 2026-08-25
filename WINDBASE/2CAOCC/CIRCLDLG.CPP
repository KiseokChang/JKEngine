#include "jkbutton.h"
#include "typectrl.h"
#include "selctrl.h"
#include "fireman.h"
#include "circldlg.h"

CircleDialog::CircleDialog(FireRecord& firerec,JKControl *parent):
	JKDialog(JKRect(340,200,700,320),parent),FireRec(firerec),
	Entry1(0,"",0,5000)
{
	WinAttr|=WA_TITLEMOVEABLE;
	SetText("¶¥Ñw Îa¸â ¬a‰b");
}

CircleDialog::~CircleDialog()
{
}

void CircleDialog::SetupWindow()
{
	AddControl(new TypeEntryControl(JKRect(100,20,310,40),ID_USERCONTROL+1,Entry1));

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

void CircleDialog::PaintClient(JKDC& dc,JKRect&)
{
	dc.IsOverWrite=FALSE;
	dc.TextOutX(JKRect(50,20,95,40),"¤e   ‰w",ADJ_RIGHT|ADJ_YCENTER);
}

void CircleDialog::EvShowWindow()
{
	JKDialog::EvShowWindow();
	JKControl *ctrl=GetControl(ID_USERCONTROL+1);
	if(ctrl) ctrl->SetFocus();
}

void CircleDialog::EvButtonClick(WORD ctrlid)
{
	if(ctrlid==ID_BUTTON+1) SendMessage(ID_OK);
	else if(ctrlid==ID_BUTTON+2) SendMessage(ID_CANCEL);
}

void CircleDialog::EvDlgOk()
{
	JKPoint32 p;
	FireRec.GetCoord(p);
	int32 radius;
	Entry1.GetNumber(radius);

	TempFireMan->AddedRecord->Assign(&FireRec);
	for(int32 step2=0;step2<radius/10;step2+=20){
		for(int32 step1=0;step1<radius/10;step1+=40){
			TempFireMan->AddedRecord->SetCoord(p+JKPoint32(step1,step2));
			TempFireMan->AddRecord(*TempFireMan->AddedRecord);
			if(step1){
				TempFireMan->AddedRecord->SetCoord(p+JKPoint32(-step1,step2));
				TempFireMan->AddRecord(*TempFireMan->AddedRecord);
			}
			if(step2){
				TempFireMan->AddedRecord->SetCoord(p+JKPoint32(step1,-step2));
				TempFireMan->AddRecord(*TempFireMan->AddedRecord);
			}
			if(step1&&step2){
				TempFireMan->AddedRecord->SetCoord(p+JKPoint32(-step1,-step2));
				TempFireMan->AddRecord(*TempFireMan->AddedRecord);
			}
		}
	}
}

void CircleDialog::EvDeadKeyDown(uint16 key,WORD /*modifier*/)
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
