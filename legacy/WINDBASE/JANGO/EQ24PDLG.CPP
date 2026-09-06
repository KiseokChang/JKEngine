#include "eq24pdlg.h"
#include "combobox.h"
#include "jkedit.h"
#include "jkstatic.h"
#include "fltbttn.h"
#include <mem.h>
#include <stdlib.h>
#include <string.h>

Eqp24InputPublicDialog::Eqp24InputPublicDialog(Equip24DataManager& e24dm,char* Budae,RPTR modify,WORD ctrlid,JKControl *parent):
	JKDialog(JKRect(200,230,700,500),parent,ctrlid),Eq24DataMan(e24dm),Modify(modify),BudaeDiscern(Budae)
{
	SetText("·© ¤e  ¢‰ ¸a");
	ClearData();
}
Eqp24InputPublicDialog::~Eqp24InputPublicDialog()
{

}

void Eqp24InputPublicDialog::InitWindow()
{
	JKDialog::InitWindow();
	if(Modify) SetValue();
	else {
		strcpy(Eqp24Data.Name24.Division,"·©¤e");
		strcpy(Eqp24Data.Name24.Attached,BudaeDiscern);
	}
}

void Eqp24InputPublicDialog::SetupWindow()
{
	JKEdit *edit=new JKEdit(JKRect(120,20,250,40),ID_EDIT+1,0,5,FALSE);
	AddControl(edit);

	edit=new JKEdit(JKRect(120,60,250,80),ID_EDIT+2,Eqp24Data.Kind24.Name,14,FALSE);
	AddControl(edit);

	edit=new JKEdit(JKRect(120,100,250,120),ID_EDIT+3,Eqp24Data.Kind24.Number,19,FALSE);
	AddControl(edit);

	edit=new JKEdit(JKRect(120,140,250,160),ID_EDIT+4,0,5,FALSE);
	AddControl(edit);

	edit=new JKEdit(JKRect(320,20,450,40),ID_EDIT+5,0,5,FALSE);
	AddControl(edit);

	edit=new JKEdit(JKRect(320,60,450,80),ID_EDIT+6,0,5,FALSE);
	AddControl(edit);

	edit=new JKEdit(JKRect(320,100,450,120),ID_EDIT+7,0,5,FALSE);
	AddControl(edit);

	JKStatic *stc=new JKStatic(JKRect(20,20,100,40),0);
	stc->SetText("®…   ¤å");
	stc->BackColor=BackColor;
	stc->AdjustFlag=ADJ_YCENTER|ADJ_RIGHT;
	AddControl(stc);

	stc=new JKStatic(JKRect(20,60,100,80),0);
	stc->SetText("Î‘   ¡w");
	stc->BackColor=BackColor;
	stc->AdjustFlag=ADJ_YCENTER|ADJ_RIGHT;
	AddControl(stc);

	stc=new JKStatic(JKRect(20,100,100,120),0);
	stc->SetText("¸‰¡¤åÑ¡");
	stc->BackColor=BackColor;
	stc->AdjustFlag=ADJ_YCENTER|ADJ_RIGHT;
	AddControl(stc);

	stc=new JKStatic(JKRect(20,140,100,160),0);
	stc->SetText("·¥  ˆa");
	stc->BackColor=BackColor;
	stc->AdjustFlag=ADJ_YCENTER|ADJ_RIGHT;
	AddControl(stc);

	stc=new JKStatic(JKRect(250,20,300,40),0);
	stc->SetText("A");
	stc->BackColor=BackColor;
	stc->AdjustFlag=ADJ_YCENTER|ADJ_RIGHT;
	AddControl(stc);

	stc=new JKStatic(JKRect(250,60,300,80),0);
	stc->SetText("B");
	stc->BackColor=BackColor;
	stc->AdjustFlag=ADJ_YCENTER|ADJ_RIGHT;
	AddControl(stc);

	stc=new JKStatic(JKRect(250,100,300,120),0);
	stc->SetText("C");
	stc->BackColor=BackColor;
	stc->AdjustFlag=ADJ_YCENTER|ADJ_RIGHT;
	AddControl(stc);

	JKFloatingButton *btn=new JKFloatingButton(JKRect(100,190,250,230),ID_BUTTON+1);
	btn->SetText("¸á¸w");
	AddControl(btn);

	btn=new JKFloatingButton(JKRect(300,190,450,230),ID_BUTTON+2);
	btn->SetText("Âá­¡");
	AddControl(btn);
}

void Eqp24InputPublicDialog::EvShowWindow()
{
	JKDialog::EvShowWindow();
	JKControl *win=GetControl(ID_EDIT+1);
	if(win) win->SetFocus();
}

void Eqp24InputPublicDialog::EvButtonClick(WORD ctrlid)
{
	switch(ctrlid){
		case ID_BUTTON+1:
			if(Modify) {
				Eq24DataMan.Eqp24Kind.PutRecord(Eqp24Data.Kind24,Modify);
			}
			else PutRecord(Eqp24Data);

			SendMessage(ID_OK);
			break;
		case ID_BUTTON+2:
			SendMessage(ID_CANCEL);
			break;
	}
}

void Eqp24InputPublicDialog::EvEditFinish(WORD ctrlid)
{
	JKEdit *edit;
	switch(ctrlid){
		case ID_EDIT+1:
			edit=(JKEdit *)GetControl(ID_EDIT+1);
			if(edit) Eqp24Data.Kind24.Order=atoi(edit->GetText());
			break;
		case ID_EDIT+4:
			edit=(JKEdit *)GetControl(ID_EDIT+4);
			if(edit) Eqp24Data.Kind24.Inga=atoi(edit->GetText());
			break;
		case ID_EDIT+5:
			edit=(JKEdit *)GetControl(ID_EDIT+5);
			if(edit) Eqp24Data.Kind24.A=atoi(edit->GetText());
			break;
		case ID_EDIT+6:
			edit=(JKEdit *)GetControl(ID_EDIT+6);
				if(edit) Eqp24Data.Kind24.B=atoi(edit->GetText());
			break;
		case ID_EDIT+7:
			edit=(JKEdit *)GetControl(ID_EDIT+7);
			if(edit) Eqp24Data.Kind24.C=atoi(edit->GetText());
			break;
	}
}

void Eqp24InputPublicDialog::PutRecord(EQP24& eqp24d)
{
	Eq24DataMan.PutRecord(eqp24d,0x0007,0x0003);
}

void Eqp24InputPublicDialog::ClearData()
{
	setmem(&Eqp24Data,sizeof(Eqp24Data),'\0');
}

void Eqp24InputPublicDialog::SetValue()
{
	char buf[6];
	JKEdit *edit;

	Eq24DataMan.Eqp24Kind.GetRecord(Modify);

	Eqp24Data.Kind24=Eq24DataMan.Eqp24Kind.KindRecord;

	edit=(JKEdit *)GetControl(ID_EDIT+2);
	if(edit) edit->SetText(Eq24DataMan.Eqp24Kind.KindRecord.Name);
	edit=(JKEdit *)GetControl(ID_EDIT+3);
	if(edit) edit->SetText(Eq24DataMan.Eqp24Kind.KindRecord.Number);
	edit=(JKEdit *)GetControl(ID_EDIT+1);
	if(edit) {
		sprintf(buf,"%u",Eq24DataMan.Eqp24Kind.KindRecord.Order);
		edit->SetText(buf);
	}
	edit=(JKEdit *)GetControl(ID_EDIT+4);
	if(edit) {
		sprintf(buf,"%u",Eq24DataMan.Eqp24Kind.KindRecord.Inga);
		edit->SetText(buf);
	}
	edit=(JKEdit *)GetControl(ID_EDIT+5);
	if(edit) {
		sprintf(buf,"%u",Eq24DataMan.Eqp24Kind.KindRecord.A);
		edit->SetText(buf);
	}
	edit=(JKEdit *)GetControl(ID_EDIT+6);
	if(edit) {
		sprintf(buf,"%u",Eq24DataMan.Eqp24Kind.KindRecord.B);
		edit->SetText(buf);
	}
	edit=(JKEdit *)GetControl(ID_EDIT+7);
	if(edit) {
		sprintf(buf,"%u",Eq24DataMan.Eqp24Kind.KindRecord.C);
		edit->SetText(buf);
	}
}

void Eqp24InputPublicDialog::EvDeadKeyDown(uint16 key,WORD /*modifier*/)
{
	WORD ctrlid;
	JKControl* ctrl;
	WORD LastCtrlID = ID_EDIT+7;
	if(key==PGDNKEY){
		if(FindSubWindow(MsgFilter->FocusWinID,ctrlid,TRUE)&&ctrlid){
			if(ctrlid>ID_EDIT+1&&ctrlid<LastCtrlID) ctrlid++;
			else if(ctrlid==ID_EDIT+1) ctrlid++;
			else if(ctrlid==LastCtrlID) ctrlid=ID_BUTTON+1;
			else if(ctrlid==ID_BUTTON+1) ctrlid=ID_BUTTON+2;
			else if(ctrlid==ID_BUTTON+2) ctrlid=ID_EDIT+1;
			else ctrlid=ID_BUTTON+1;

			ctrl=GetControl(ctrlid);
			if(ctrl) ctrl->SetFocus();
		}
	}
	else if(key==PGUPKEY){
		if(FindSubWindow(MsgFilter->FocusWinID,ctrlid,TRUE)&&ctrlid){
			if(ctrlid>ID_EDIT+1&&ctrlid<LastCtrlID) ctrlid--;
			else if(ctrlid==LastCtrlID)	ctrlid--;
			else if(ctrlid==ID_EDIT+1)	ctrlid=ID_BUTTON+2;
			else if(ctrlid==ID_BUTTON+1) ctrlid=LastCtrlID;
			else if(ctrlid==ID_BUTTON+2) ctrlid=ID_BUTTON+1;
			else ctrlid=ctrlid--;

			ctrl=GetControl(ctrlid);
			if(ctrl) ctrl->SetFocus();
		}
	}
}
