#include "eq24somo.h"
#include "jkedit.h"
#include "jkstatic.h"
#include "fltbttn.h"
#include <mem.h>
#include <stdlib.h>
#include <string.h>

Eqp24InputSomoDialog::Eqp24InputSomoDialog(Equip24DataManager& e24dm,RPTR modify,BOOL somoflag,WORD ctrlid,JKControl *parent):
	JKDialog(JKRect(200,200,700,470),parent,ctrlid),Eq24DataMan(e24dm),Modify(modify),SomoFlag(somoflag)
{
	if(SomoFlag) SetText("­¡¡¡ £¹ ¬b¹A");
	else SetText("¤e  s");
	ClearData();
}
Eqp24InputSomoDialog::~Eqp24InputSomoDialog()
{
}

void Eqp24InputSomoDialog::InitWindow()
{
	JKDialog::InitWindow();
	SetValue();
}

void Eqp24InputSomoDialog::SetupWindow()
{


	JKEdit *edit=new JKEdit(JKRect(320,20,450,40),ID_EDIT+1,0,5,FALSE);
	edit->SetInsertMode(FALSE);
	AddControl(edit);

	edit=new JKEdit(JKRect(320,60,450,80),ID_EDIT+2,0,5,FALSE);
	edit->SetInsertMode(FALSE);
	AddControl(edit);

	edit=new JKEdit(JKRect(320,100,450,120),ID_EDIT+3,0,5,FALSE);
	edit->SetInsertMode(FALSE);
	AddControl(edit);

	edit=new JKEdit(JKRect(320,140,450,160),ID_EDIT+4,Eqp24Data.Kind24.Date,10,FALSE);
	edit->SetInsertMode(FALSE);
	AddControl(edit);

	JKStatic *stc=new JKStatic(JKRect(120,20,250,40),ID_STATIC+1);
	stc->BackColor=BackColor;
	AddControl(stc);

	stc=new JKStatic(JKRect(120,60,250,80),ID_STATIC+2);
	stc->BackColor=BackColor;
	AddControl(stc);

	stc=new JKStatic(JKRect(120,100,250,120),ID_STATIC+3);
	stc->BackColor=BackColor;
	AddControl(stc);

  stc=new JKStatic(JKRect(120,140,250,160),ID_STATIC+4);
	stc->BackColor=BackColor;
	AddControl(stc);

	stc=new JKStatic(JKRect(20,20,100,40),0);
	stc->SetText("Î‘   ¡w :");
	stc->BackColor=BackColor;
	stc->AdjustFlag=ADJ_YCENTER|ADJ_RIGHT;
	AddControl(stc);

	stc=new JKStatic(JKRect(20,60,100,80),0);
	stc->SetText("¸‰¡¤åÑ¡ :");
	stc->BackColor=BackColor;
	stc->AdjustFlag=ADJ_YCENTER|ADJ_RIGHT;
	AddControl(stc);

	stc=new JKStatic(JKRect(20,100,100,120),0);
  stc->SetText("®…  ¤å :");
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

  stc=new JKStatic(JKRect(200,140,300,160),ID_STATIC+5);
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

void Eqp24InputSomoDialog::EvShowWindow()
{
	JKDialog::EvShowWindow();
	JKControl *ctrl=GetControl(ID_EDIT+1);
	if(ctrl) ctrl->SetFocus();
}

void Eqp24InputSomoDialog::EvButtonClick(WORD ctrlid)
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

void Eqp24InputSomoDialog::EvEditFinish(WORD ctrlid)
{
	JKEdit *edit;
	WORD tempint;
	char buf[6];

	switch(ctrlid){
		case ID_EDIT+1:
			edit=(JKEdit *)GetControl(ID_EDIT+1);
			if(edit) {
				tempint=atoi(edit->GetText());
				if(Eqp24Data2.Kind24.A<tempint){
					sprintf(buf,"%u",Eqp24Data.Kind24.A);
					edit->SetText(buf);
					edit->SetFocus();
				}
				else Eqp24Data.Kind24.A=tempint;
			}
			break;
		case ID_EDIT+2:
			edit=(JKEdit *)GetControl(ID_EDIT+2);
				if(edit) {
					tempint=atoi(edit->GetText());
					if(Eqp24Data2.Kind24.B<tempint){
						sprintf(buf,"%u",Eqp24Data.Kind24.B);
						edit->SetText(buf);
						edit->SetFocus();
					}
					else Eqp24Data.Kind24.B=tempint;
				}
			break;
		case ID_EDIT+3:
			edit=(JKEdit *)GetControl(ID_EDIT+3);
			if(edit) {
				tempint=atoi(edit->GetText());
				if(Eqp24Data2.Kind24.C<tempint){
					sprintf(buf,"%u",Eqp24Data.Kind24.C);
					edit->SetText(buf);
					edit->SetFocus();
				}
				else Eqp24Data.Kind24.C=tempint;
			}
			break;
	}
}

void Eqp24InputSomoDialog::ClearData()
{
	setmem(&Eqp24Data,sizeof(Eqp24Data),'\0');
}

void Eqp24InputSomoDialog::SetValue()
{
	char buf[6];
	date Date;
	JKEdit *edit;
	JKControl *ctrl;

	Eq24DataMan.Eqp24Kind.GetRecord(Modify);

	Eqp24Data.Kind24=Eq24DataMan.Eqp24Kind.KindRecord;
	Eqp24Data2.Kind24=Eq24DataMan.Eqp24Kind.KindRecord;
	Eqp24Data.Kind24.A=Eqp24Data.Kind24.B=Eqp24Data.Kind24.C=0;

	ctrl=GetControl(ID_STATIC+1);
	if(ctrl) ctrl->SetText(Eq24DataMan.Eqp24Kind.KindRecord.Name);

	ctrl=GetControl(ID_STATIC+2);
	if(ctrl) ctrl->SetText(Eq24DataMan.Eqp24Kind.KindRecord.Number);

	ctrl=GetControl(ID_STATIC+3);
	if(ctrl) {
		sprintf(buf,"%u",Eq24DataMan.Eqp24Kind.KindRecord.Order);
		ctrl->SetText(buf);
	}
	ctrl=GetControl(ID_STATIC+4);
	if(ctrl) {
		sprintf(buf,"%u",Eq24DataMan.Eqp24Kind.KindRecord.Inga);
		ctrl->SetText(buf);
	}

  ctrl=GetControl(ID_STATIC+5);
	if(ctrl) {
		if(SomoFlag) ctrl->SetText("­¡¡¡/¬b¹A ·©");
		else ctrl->SetText("¤es ·©");
	}

	edit=(JKEdit *)GetControl(ID_EDIT+1);
	if(edit) {
		strcpy(buf,"0");
		edit->SetText(buf);
	}
	edit=(JKEdit *)GetControl(ID_EDIT+2);
	if(edit) {
		strcpy(buf,"0");
		edit->SetText(buf);
	}
	edit=(JKEdit *)GetControl(ID_EDIT+3);
	if(edit) {
		strcpy(buf,"0");
		edit->SetText(buf);
	}
	edit=(JKEdit *)GetControl(ID_EDIT+4);
	if(edit) {
		getdate(&Date);
		sprintf(Eq24DataMan.Eqp24Kind.KindRecord.Date,"%4d.%2d.%2d",Date.da_year,Date.da_mon,Date.da_day);
		edit->SetText(Eq24DataMan.Eqp24Kind.KindRecord.Date);
	}
}


void Eqp24InputSomoDialog::SaveFunction()
{
	Eq24DataMan.Eqp24Kind.GetRecord(Modify);
	Eq24DataMan.Eqp24Name.GetRecord(Eqp24Data.Name24,Eq24DataMan.Eqp24Kind.KindRecord.NameIndex);

	Eq24DataMan.Eqp24Kind.KindRecord.A-=Eqp24Data.Kind24.A;
	Eq24DataMan.Eqp24Kind.KindRecord.B-=Eqp24Data.Kind24.B;
	Eq24DataMan.Eqp24Kind.KindRecord.C-=Eqp24Data.Kind24.C;

	Eq24DataMan.Eqp24Kind.PutRecord(Eq24DataMan.Eqp24Kind.KindRecord,Modify);

	if(SomoFlag){
		Eq24DataMan.Eqp24Name.ChangeFileName(EQ24SNAME);
		Eq24DataMan.Eqp24Kind.ChangeFileName(EQ24SKIND);
	}
	else {
		Eq24DataMan.Eqp24Name.ChangeFileName(EQ24RNAME);
		Eq24DataMan.Eqp24Kind.ChangeFileName(EQ24RKIND);
	}

	if(Eqp24Data.Kind24.A||Eqp24Data.Kind24.B||Eqp24Data.Kind24.C) {
		Eq24DataMan.PutRecord(Eqp24Data,0x0007,0x0007);
	}

	Eq24DataMan.Eqp24Name.ChangeFileName(EQP24NAME);
	Eq24DataMan.Eqp24Kind.ChangeFileName(EQP24KIND);
}

void Eqp24InputSomoDialog::EvDeadKeyDown(uint16 key,WORD /*modifier*/)
{
	WORD ctrlid;
	JKControl* ctrl;
	WORD LastCtrlID = ID_EDIT+4;
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
