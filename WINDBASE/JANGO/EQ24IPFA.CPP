#include "eq24ipfa.h"
#include "eq24ipkd.h"
#include "combobox.h"
#include "jkedit.h"
#include "scrllist.h"
#include "jkstatic.h"
#include "fltbttn.h"
#include <mem.h>
#include <stdlib.h>
#include <string.h>

Equip24InputFA::Equip24InputFA(Equip24DataManager& eqp24dm,WORD sellect,char *text,char *budae,WORD ctrlid,JKControl *parent):
	JKDialog(JKRect(300,200,600,450),parent,ctrlid),Eqp24DMan(eqp24dm),Sellect(sellect),Eqp24Kind(0),BudaeDiscern(budae)
{
	SetText(text);
	ClearData();
	strcpy(Equip24Data.Name24.Attached,BudaeDiscern);
  strcpy(Equip24Data.Name24.Division,"ˆ·¥");
}
Equip24InputFA::~Equip24InputFA()
{
	Eqp24DMan.Eqp24Kind.ChangeFileName(EQP24KIND);
}

void Equip24InputFA::InitWindow()
{
	JKDialog::InitWindow();
}

void Equip24InputFA::SetupWindow()
{
	JKStatic *stc=new JKStatic(JKRect(20,20,70,40),ID_STATIC+1);
	stc->AdjustFlag=ADJ_YCENTER|ADJ_RIGHT;
	stc->BackColor=BackColor;
	stc->SetText("¬÷¡w");
	AddControl(stc);

	stc=new JKStatic(JKRect(20,70,70,90),ID_STATIC+2);
	stc->AdjustFlag=ADJ_YCENTER|ADJ_RIGHT;
	stc->BackColor=BackColor;
	stc->SetText("Š…¤å");
	AddControl(stc);

	JKEdit *edit=new JKEdit(JKRect(80,20,230,40),ID_EDIT+1,Equip24Data.Name24.Name,8,FALSE);
	AddControl(edit);

	edit=new JKEdit(JKRect(80,70,230,90),ID_EDIT+2,Equip24Data.Name24.Number,11,FALSE);
	AddControl(edit);

	JKFloatingButton *fbtn=new JKFloatingButton(JKRect(30,160,130,200),ID_BUTTON+1);
	fbtn->SetText("¸á¸w");
	AddControl(fbtn);

	fbtn=new JKFloatingButton(JKRect(160,160,260,200),ID_BUTTON+2);
	fbtn->SetText("¹·ža");
	AddControl(fbtn);
}

void Equip24InputFA::EvShowWindow()
{
	JKDialog::EvShowWindow();
	JKControl *win=GetControl(ID_EDIT+1);
	if(win) win->SetFocus();
}

void Equip24InputFA::EvButtonClick(WORD ctrlid)
{
	switch(ctrlid){
		case ID_BUTTON+1:
			switch(Sellect) {
				case 0:
					Junib();
				break;
				case 1:
					JunChul();
				break;
				case 2:
					JunYouk();
				break;
			}
			SendMessage(ID_OK);
			break;
		case ID_BUTTON+2:
			SendMessage(ID_CANCEL);
			break;
	}
}

void Equip24InputFA::ClearData()
{
	setmem(&Equip24Data,sizeof(EQP24),'\0');
}

void Equip24InputFA::EvDeadKeyDown(uint16 key,WORD /*modifier*/)
{
	WORD ctrlid;
	JKControl* ctrl;
	WORD LastCtrlID = ID_EDIT+2;
	if(key==PGDNKEY){
		if(FindSubWindow(MsgFilter->FocusWinID,ctrlid,TRUE)&&ctrlid){
			if(ctrlid>ID_EDIT+1&&ctrlid<LastCtrlID) ctrlid++;
			else if(ctrlid==LastCtrlID) ctrlid=ID_BUTTON+1;
			else if(ctrlid==ID_BUTTON+1) ctrlid=ID_BUTTON+2;
			else if(ctrlid==ID_BUTTON+2) ctrlid=ID_EDIT+1;
			else if(ctrlid==ID_EDIT+1) ctrlid++;
			else ctrlid=ID_EDIT+1;

			ctrl=GetControl(ctrlid);
			if(ctrl) ctrl->SetFocus();
		}
	}
	else if(key==PGUPKEY){
		if(FindSubWindow(MsgFilter->FocusWinID,ctrlid,TRUE)&&ctrlid){
			if(ctrlid>ID_EDIT+1&&ctrlid<LastCtrlID) ctrlid--;
			else if(ctrlid==LastCtrlID)	ctrlid--;
			else if(ctrlid==ID_EDIT+1)	ctrlid=ID_BUTTON+2;
			else if(ctrlid==ID_BUTTON+2)	ctrlid=ID_BUTTON+1;
			else if(ctrlid==ID_BUTTON+1)	ctrlid=LastCtrlID;
			else ctrlid=ID_EDIT+1;

			ctrl=GetControl(ctrlid);
			if(ctrl) ctrl->SetFocus();
		}
	}
}

void Equip24InputFA::Junib()
{
	RPTR index=1;
	Eqp24Kind.ChangeFileName(EQP24001);
	while(index<Eqp24Kind.DFileMan.FileHeader[Eqp24Kind.FilePoint].Record_Count){
		if(Eqp24Kind.GetRecord(index)){
			Equip24Data.Kind24=Eqp24Kind.KindRecord;
			Eqp24DMan.PutRecord(Equip24Data);
		}
		index++;
	}
	index=1;
	Eqp24Kind.ChangeFileName(EQP24002);
	while(index<Eqp24Kind.DFileMan.FileHeader[Eqp24Kind.FilePoint].Record_Count){
		if(Eqp24Kind.GetRecord(index)){
			Equip24Data.Kind24=Eqp24Kind.KindRecord;
			Eqp24DMan.PutRecord(Equip24Data);
			ChangeJaego("¹A‰¡",Eqp24Kind.KindRecord.Name,Eqp24Kind.KindRecord.A,Eqp24Kind.KindRecord.B,Eqp24Kind.KindRecord.C,FALSE);
		}
		index++;
	}
}

void Equip24InputFA::JunChul()
{
	RPTR index=Eqp24DMan.Eqp24Name.Search(Equip24Data.Name24,0,0x000C);
	if(index!=-1)	Eqp24DMan.DeleteRecord(index,1);

	Eqp24Kind.ChangeFileName(EQP24002);
	while(index<Eqp24Kind.DFileMan.FileHeader[Eqp24Kind.FilePoint].Record_Count){
		if(Eqp24Kind.GetRecord(index)){
			//Equip24Data.Kind24=Eqp24Kind.KindRecord;
			//Eqp24DMan.PutRecord(Equip24Data);
			ChangeJaego("¹A‰¡",Eqp24Kind.KindRecord.Name,Eqp24Kind.KindRecord.A,Eqp24Kind.KindRecord.B,Eqp24Kind.KindRecord.C,TRUE);
		}
		index++;
	}
}

void Equip24InputFA::JunYouk()
{
	RPTR index=Eqp24DMan.Eqp24Name.Search(Equip24Data.Name24,0,0x000C);
	if(index!=-1)	Eqp24DMan.DeleteRecord(index,1);

	Eqp24Kind.ChangeFileName(EQP24003);
	while(index<Eqp24Kind.DFileMan.FileHeader[Eqp24Kind.FilePoint].Record_Count){
		if(Eqp24Kind.GetRecord(index)){
			//Equip24Data.Kind24=Eqp24Kind.KindRecord;
			//Eqp24DMan.PutRecord(Equip24Data);
      ChangeJaego("¸‰¡",Eqp24Kind.KindRecord.Name,Eqp24Kind.KindRecord.A,Eqp24Kind.KindRecord.B,Eqp24Kind.KindRecord.C,TRUE);
		}
		index++;
	}
}



BOOL Equip24InputFA::ChangeJaego(char *nnaem,char *kname,int16 A,int16 B,int16 C,BOOL add)
{
	RPTR index=Eqp24DMan.Eqp24Name.Search(nnaem,0,0x0004);
	if(index!=-1) {
		Eqp24DMan.Eqp24Name.GetRecord(index);
		index=Eqp24DMan.Eqp24Name.NameRecord.FirstIndex;

		while(index!=-1){
			Eqp24DMan.Eqp24Kind.GetRecord(index);
			if(Eqp24DMan.Eqp24Kind.Compare(Eqp24DMan.Eqp24Kind.KindRecord,kname,0x0001)){
				if(add) {
					Eqp24DMan.Eqp24Kind.KindRecord.A+=A;
					Eqp24DMan.Eqp24Kind.KindRecord.B+=B;
					Eqp24DMan.Eqp24Kind.KindRecord.C+=C;
				}
				else {
					if(Eqp24DMan.Eqp24Kind.KindRecord.A==0) B+=A;
					else if(Eqp24DMan.Eqp24Kind.KindRecord.A<abs(A)){
						A+=Eqp24DMan.Eqp24Kind.KindRecord.A;
						B+=A;
						Eqp24DMan.Eqp24Kind.KindRecord.A=0;
					}
					if(Eqp24DMan.Eqp24Kind.KindRecord.B==0) C+=B;
					else if(Eqp24DMan.Eqp24Kind.KindRecord.B<abs(B)){
						B+=Eqp24DMan.Eqp24Kind.KindRecord.B;
						C+=B;
						Eqp24DMan.Eqp24Kind.KindRecord.B=0;
					}
					Eqp24DMan.Eqp24Kind.KindRecord.C+=C;
				}
				Eqp24DMan.Eqp24Kind.PutRecord(Eqp24DMan.Eqp24Kind.KindRecord,index);
				return TRUE;
			}
			index=Eqp24DMan.Eqp24Kind.KindRecord.NextIndex;
		}
	}
	return FALSE;
}

