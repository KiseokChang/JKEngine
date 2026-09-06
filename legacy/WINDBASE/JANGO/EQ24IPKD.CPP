#include "eq24ipkd.h"
#include "jkedit.h"
#include "jkstatic.h"
#include "fltbttn.h"
#include <mem.h>
#include <stdlib.h>
#include <string.h>

Eqp24InputKindDialog::Eqp24InputKindDialog(Equip24Kind &eq24kind,RPTR modify,WORD ctrlid,JKControl *parent):
	JKDialog(JKRect(200,200,700,470),parent,ctrlid),Eqp24Kind(eq24kind),Modify(modify)
{
	ClearData();
}

Eqp24InputKindDialog::~Eqp24InputKindDialog()
{
}

void Eqp24InputKindDialog::InitWindow()
{
	JKDialog::InitWindow();
	if(Modify) SetValue();
}

void Eqp24InputKindDialog::SetupWindow()
{
	JKEdit *edit=new JKEdit(JKRect(120,20,250,40),ID_EDIT+1,0,5,FALSE);
	edit->SetInsertMode(FALSE);
	AddControl(edit);

	edit=new JKEdit(JKRect(120,60,250,80),ID_EDIT+2,Kind24.Name,14,FALSE);
	edit->SetInsertMode(FALSE);
	AddControl(edit);

	edit=new JKEdit(JKRect(120,100,250,120),ID_EDIT+3,Kind24.Number,19,FALSE);
	edit->SetInsertMode(FALSE);
	AddControl(edit);

	edit=new JKEdit(JKRect(120,140,250,160),ID_EDIT+4,0,5,FALSE);
	edit->SetInsertMode(FALSE);
	AddControl(edit);

	edit=new JKEdit(JKRect(320,20,450,40),ID_EDIT+5,0,5,FALSE);
	edit->SetInsertMode(FALSE);
	AddControl(edit);

	edit=new JKEdit(JKRect(320,60,450,80),ID_EDIT+6,0,5,FALSE);
	edit->SetInsertMode(FALSE);
	AddControl(edit);

	edit=new JKEdit(JKRect(320,100,450,120),ID_EDIT+7,0,5,FALSE);
	edit->SetInsertMode(FALSE);
	AddControl(edit);

	JKStatic *stc=new JKStatic(JKRect(20,20,100,40),0);
	stc->SetText("®…  ¤å");
	stc->BackColor=BackColor;
	stc->AdjustFlag=ADJ_YCENTER|ADJ_RIGHT;
	AddControl(stc);

	stc=new JKStatic(JKRect(20,60,100,80),0);
	stc->SetText("Î‘  ¡w");
	stc->BackColor=BackColor;
	stc->AdjustFlag=ADJ_YCENTER|ADJ_RIGHT;
	AddControl(stc);

	stc=new JKStatic(JKRect(20,100,100,120),0);
	stc->SetText("¹A‰¡¤åÑ¡");
	stc->BackColor=BackColor;
	stc->AdjustFlag=ADJ_YCENTER|ADJ_RIGHT;
	AddControl(stc);

	stc=new JKStatic(JKRect(20,140,100,160),0);
	stc->SetText("·¥  ˆa :");
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

	JKFloatingButton *btn=new JKFloatingButton(JKRect(100,180,250,220),ID_BUTTON+1);
	btn->SetText("¸á¸w");
	AddControl(btn);

	btn=new JKFloatingButton(JKRect(300,180,450,220),ID_BUTTON+2);
	btn->SetText("Âá­¡");
	AddControl(btn);
}

void Eqp24InputKindDialog::EvShowWindow()
{
	JKDialog::EvShowWindow();
	JKControl *ctrl=GetControl(ID_EDIT+1);
	if(ctrl) ctrl->SetFocus();
}

void Eqp24InputKindDialog::EvButtonClick(WORD ctrlid)
{
	switch(ctrlid){
		case ID_BUTTON+1:
			SaveFunction();
			SendMessage(ID_OK);
			break;
		case ID_BUTTON+2:
			SendMessage(ID_CANCEL);
			break;
	}
}

void Eqp24InputKindDialog::EvEditFinish(WORD ctrlid)
{
	JKEdit *edit;
	WORD tempint;
	char buf[6];

	switch(ctrlid){
		case ID_EDIT+1:
			edit=(JKEdit *)GetControl(ID_EDIT+1);
			if(edit) {
				Kind24.Order=atoi(edit->GetText());
			}
			break;
		case ID_EDIT+4:
			edit=(JKEdit *)GetControl(ID_EDIT+4);
			if(edit) {
				Kind24.Inga=atoi(edit->GetText());
			}
			break;
		case ID_EDIT+5:
			edit=(JKEdit *)GetControl(ID_EDIT+5);
			if(edit) {
				Kind24.A=atoi(edit->GetText());
			}
			break;
		case ID_EDIT+6:
			edit=(JKEdit *)GetControl(ID_EDIT+6);
			if(edit) {
				Kind24.B=atoi(edit->GetText());
			}
			break;
		case ID_EDIT+7:
			edit=(JKEdit *)GetControl(ID_EDIT+7);
			if(edit) {
				Kind24.C=atoi(edit->GetText());
			}
			break;
	}
}

void Eqp24InputKindDialog::ClearData()
{
	setmem(&Kind24,sizeof(Kind24),'\0');
}

void Eqp24InputKindDialog::SetValue()
{
	char buf[6];
	date Date;
	JKEdit *edit;
	JKControl *ctrl;

	Eqp24Kind.GetRecord(Modify);

	Kind24=Eqp24Kind.KindRecord;

	edit=(JKEdit *)GetControl(ID_EDIT+1);
	if(edit) {
		sprintf(buf,"%u",Kind24.Order);
		edit->SetText(buf);
	}
	edit=(JKEdit *)GetControl(ID_EDIT+2);
	if(edit) edit->SetText(Kind24.Name);

	edit=(JKEdit *)GetControl(ID_EDIT+3);
	if(edit) edit->SetText(Kind24.Number);

	edit=(JKEdit *)GetControl(ID_EDIT+4);
	if(edit) {
		sprintf(buf,"%u",Kind24.Inga);
		edit->SetText(buf);
	}
	edit=(JKEdit *)GetControl(ID_EDIT+5);
	if(edit) {
		sprintf(buf,"%u",Kind24.A);
		edit->SetText(buf);
	}
	edit=(JKEdit *)GetControl(ID_EDIT+6);
	if(edit) {
		sprintf(buf,"%u",Kind24.B);
		edit->SetText(buf);
	}
	edit=(JKEdit *)GetControl(ID_EDIT+7);
	if(edit) {
		sprintf(buf,"%u",Kind24.C);
		edit->SetText(buf);
	}
}

void Eqp24InputKindDialog::EvDeadKeyDown(uint16 key,WORD /*modifier*/)
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

void Eqp24InputKindDialog::SaveFunction()
{
	if(Modify)Eqp24Kind.PutRecord(Kind24,Modify);
	else Eqp24Kind.PutRecord(Kind24);
}