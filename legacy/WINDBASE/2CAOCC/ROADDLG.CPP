#include <math.h>
#include "jkbutton.h"
#include "typectrl.h"
#include "calccord.h"
#include "fireman.h"
#include "roaddlg.h"

RoadDialog::RoadDialog(FireRecord& firerec,JKControl *parent):
	JKDialog(JKRect(340,200,700,320),parent),FireRec(firerec),
	Entry1(0,"",0,1000),Entry2(1,"",0,6400)
{
	WinAttr|=WA_TITLEMOVEABLE;
	SetText("•¡¡ Îa¸â ¬a‰b");
}

RoadDialog::~RoadDialog()
{
}

void RoadDialog::SetupWindow()
{
	AddControl(new TypeEntryControl(JKRect(100,20,310,40),ID_USERCONTROL+1,Entry2));

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

void RoadDialog::PaintClient(JKDC& dc,JKRect&)
{
	dc.IsOverWrite=FALSE;
//	dc.TextOutX(JKRect(50,20,95,40),"‹©       ·¡",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(50,20,95,40),"¸wÂ‚ ¤w¶áˆb",ADJ_RIGHT|ADJ_YCENTER);
}

void RoadDialog::EvShowWindow()
{
	JKDialog::EvShowWindow();
	JKControl *ctrl=GetControl(ID_USERCONTROL+1);
	if(ctrl) ctrl->SetFocus();
}

void RoadDialog::EvButtonClick(WORD ctrlid)
{
	if(ctrlid==ID_BUTTON+1) SendMessage(ID_OK);
	else if(ctrlid==ID_BUTTON+2) SendMessage(ID_CANCEL);
}

void RoadDialog::EvDlgOk()
{
	JKPoint32 p;
	FireRec.GetCoord(p);
//	int32 lenngth=atoi(Entry1.Data);
	int32 mil;
	Entry2.GetNumber(mil);

	TempFireMan->AddedRecord->Assign(&FireRec);
	CoordCalculator Coordalc;
	double radian=Coordalc.MilToRadian(mil);

	for(int16 step=0;step<=50;step+=25){
		if(step){
			JKPoint32 diff(step*cos(M_PI/2.-radian),step*sin(M_PI/2.-radian));

			TempFireMan->AddedRecord->SetCoord(p+diff);
			TempFireMan->AddRecord(*TempFireMan->AddedRecord);

			TempFireMan->AddedRecord->SetCoord(p-diff);
			TempFireMan->AddRecord(*TempFireMan->AddedRecord);
		}
		else{
			TempFireMan->AddedRecord->SetCoord(p);
			TempFireMan->AddRecord(*TempFireMan->AddedRecord);
		}
	}
}

void RoadDialog::EvDeadKeyDown(uint16 key,WORD /*modifier*/)
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
