#include "jkbutton.h"
#include "okdialog.h"
#include "typectrl.h"
#include "selctrl.h"
#include "plntgman.h"
#include "tgman.h"
#include "tgaq1dlg.h"

TargetAcquisition1Dialog::TargetAcquisition1Dialog(FireRecord& firerec,JKControl *parent):
	JKDialog(JKRect(340,200,700,320),parent),FireRec(firerec)
{
	WinAttr|=WA_TITLEMOVEABLE;
	SetText("Îa¸â ¤åÑ¡ ·³b");
}

TargetAcquisition1Dialog::~TargetAcquisition1Dialog()
{
}

void TargetAcquisition1Dialog::SetupWindow()
{
	AddControl(new TypeEntryControl(JKRect(100, 20,310, 40),ID_USERCONTROL+ 1,*FireRec.GetEntry(1)));

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

void TargetAcquisition1Dialog::PaintClient(JKDC& dc,JKRect&)
{
	dc.IsOverWrite=FALSE;
	dc.TextOutX(JKRect(50, 20,95, 40),"Îa¸â¤åÑ¡",ADJ_RIGHT|ADJ_YCENTER);
}

void TargetAcquisition1Dialog::EvShowWindow()
{
	JKDialog::EvShowWindow();
	JKControl *ctrl=GetControl(ID_USERCONTROL+1);
	if(ctrl) ctrl->SetFocus();
}

void TargetAcquisition1Dialog::EvButtonClick(WORD ctrlid)
{
	if(ctrlid==ID_BUTTON+1) {
		EntryBase *ent=FireRec.GetEntry(1)->Duplicate();
		EntryBase *ent1=FireRec.GetEntry(2)->Duplicate();
		EntryBase *ent2=FireRec.GetEntry(3)->Duplicate();
		EntryBase *ent3=FireRec.GetEntry(4)->Duplicate();
		EntryBase *ent4=FireRec.GetEntry(5)->Duplicate();
		if(ent&&ent1&&ent2&&ent3&&ent4){
			ent->EntryID=0;
			ent1->EntryID=1; ent2->EntryID=2; ent3->EntryID=3; ent4->EntryID=4;
			int16 index;
			index=TargetMan->Search(ent);
			if(index>=0){
				TargetMan->RecordMan.GetRecord(index,*TargetMan->AddedRecord);
				ent1->Assign(TargetMan->AddedRecord->GetEntry(1));
				ent2->Assign(TargetMan->AddedRecord->GetEntry(2));
				ent3->Assign(TargetMan->AddedRecord->GetEntry(3));
				ent4->Assign(TargetMan->AddedRecord->GetEntry(4));

				ent1->EntryID=2; ent2->EntryID=3; ent3->EntryID=4; ent4->EntryID=5;

				FireRec.GetEntry(2)->Assign(ent1);
				FireRec.GetEntry(3)->Assign(ent2);
				FireRec.GetEntry(4)->Assign(ent3);
				FireRec.GetEntry(5)->Assign(ent4);
				SendMessage(ID_OK);
			}
			else{
				index=PlanTargetMan->Search(ent);
				if(index>=0){
					PlanTargetMan->RecordMan.GetRecord(index,*PlanTargetMan->AddedRecord);
					ent1->Assign(PlanTargetMan->AddedRecord->GetEntry(1));
					ent2->Assign(PlanTargetMan->AddedRecord->GetEntry(2));
					ent3->Assign(PlanTargetMan->AddedRecord->GetEntry(3));
					ent4->Assign(PlanTargetMan->AddedRecord->GetEntry(4));

					ent1->EntryID=2; ent2->EntryID=3; ent3->EntryID=4; ent4->EntryID=5;

					FireRec.GetEntry(2)->Assign(ent1);
					FireRec.GetEntry(3)->Assign(ent2);
					FireRec.GetEntry(4)->Assign(ent3);
					FireRec.GetEntry(5)->Assign(ent4);
					SendMessage(ID_OK);
				}
				else{
					OKDialog(JKRect(300,200,700,400),"Ð”w Îa¸â¤åÑ¡ˆa ´ô¯s“¡”a.").Run();
					SendMessage(ID_CANCEL);
				}
			}
		}
		if(ent)  delete ent;
		if(ent1) delete ent1;	if(ent2) delete ent2;
		if(ent3) delete ent3;	if(ent4) delete ent4;
	}
	else if(ctrlid==ID_BUTTON+2) SendMessage(ID_CANCEL);
}

void TargetAcquisition1Dialog::EvDeadKeyDown(uint16 key,WORD /*modifier*/)
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
