#include "yesnodlg.h"
#include "jkbutton.h"
#include "typectrl.h"
#include "selctrl.h"
#include "plntgman.h"
#include "tgman.h"
#include "tgaq2dlg.h"

TargetAcquisition2Dialog::TargetAcquisition2Dialog(FireRecord& firerec,BOOL issearch,JKControl *parent):
	JKDialog(JKRect(340,200,700,330),parent),FireRec(firerec),IsSearch(issearch)
{
	WinAttr|=WA_TITLEMOVEABLE;
	SetText("Îa¸â ¹ÁÎa ·³b");
}

TargetAcquisition2Dialog::~TargetAcquisition2Dialog()
{
}

void TargetAcquisition2Dialog::SetupWindow()
{
	AddControl(new TypeEntryControl(JKRect(100, 20,310, 40),ID_USERCONTROL+ 1,*FireRec.GetEntry(2)));
	AddControl(new SelectEntryControl(JKRect(100, 45,310,65),ID_USERCONTROL+ 4,*FireRec.GetEntry(3)));

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

void TargetAcquisition2Dialog::PaintClient(JKDC& dc,JKRect&)
{
	dc.IsOverWrite=FALSE;
	dc.TextOutX(JKRect(50,20,95,40),"¹ÁÎa"    ,ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(50,45,95,65),"Îa¸â¬÷»©",ADJ_RIGHT|ADJ_YCENTER);
}

void TargetAcquisition2Dialog::EvShowWindow()
{
	JKDialog::EvShowWindow();
	JKControl *ctrl=GetControl(ID_USERCONTROL+1);
	if(ctrl) ctrl->SetFocus();
}

void TargetAcquisition2Dialog::EvButtonClick(WORD ctrlid)
{
	if(ctrlid==ID_BUTTON+1) {
		if(IsSearch){
			EntryBase *ent =FireRec.GetEntry(1)->Duplicate();
			EntryBase *ent1=FireRec.GetEntry(2)->Duplicate();
			EntryBase *ent2=FireRec.GetEntry(3)->Duplicate();
			if(ent&&ent1&&ent2){
				ent->EntryID=0; ent1->EntryID=1; ent2->EntryID=2;

				int16 index;
				JKPoint32 coord;
				FireRec.GetCoord(coord);
				index=TargetMan->SearchNearCoord(coord,5);
				if(index>=0){
					TargetMan->RecordMan.GetRecord(index,*TargetMan->AddedRecord);
					if(YesNoDialog(JKRect(300,200,700,400),"º´á»¥ ¹ÁÎa· ¤e‰w 50m ´eµA\n"
																						 "‰ÒB–E Îa¸â·¡ ¹¥¸Ðs“¡”a\n"
																						 "·¡ Îa¸â·a¡ ¬a‰bÐs“¡Œa?").Run()==ID_YES){
						ent->Assign(TargetMan->AddedRecord->GetEntry((uint16)0));
						ent1->Assign(TargetMan->AddedRecord->GetEntry(1));
						ent2->Assign(TargetMan->AddedRecord->GetEntry(2));

						ent->EntryID=1; ent1->EntryID=2; ent2->EntryID=3;

						FireRec.Assign(ent); FireRec.Assign(ent1); FireRec.Assign(ent2);
					}
					else FireRec.GetEntry(1)->InitData();
				}
			}
			if(ent)  delete ent; if(ent1) delete ent1;	if(ent2) delete ent2;
		}
		SendMessage(ID_OK);
	}
	else if(ctrlid==ID_BUTTON+2) SendMessage(ID_CANCEL);
}

void TargetAcquisition2Dialog::EvDeadKeyDown(uint16 key,WORD /*modifier*/)
{
	WORD ctrlid;
	JKControl* ctrl;
	WORD LastCtrlID = ID_USERCONTROL+2;
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
