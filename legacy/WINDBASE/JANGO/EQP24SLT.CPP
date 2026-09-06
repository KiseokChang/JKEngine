#include "eqp24slt.h"
#include "combobox.h"
#include "jkstatic.h"
#include "fltbttn.h"

Equip24SellectDialog::Equip24SellectDialog(Equip24DataManager& e24dm,RPTR modify,WORD ctrlid,JKControl *parent):
	JKDialog(JKRect(200,200,700,520),parent,ctrlid),Eq24DataMan(e24dm),Modify(modify)
{
	ClearData();
}
Equip24SellectDialog::~Equip24SellectDialog()
{

}

void Equip24SellectDialog::InitWindow()
{
	JKDialog::InitWindow();
	strcpy(Eqp24Data.Name24.Division,"ˆ·¥");
}

void Equip24SellectDialog::SetupWindow()
{
	Combobox *Name=new Combobox(JKRect(120,20,250,110),ID_EDIT,6);
	AddControl(Name);
	Name->HuboMan->Add(0,new HuboBase(EQPDAEDAEHQ));
	Name->HuboMan->Add(1,new HuboBase(EQPDAEDAEA));
	Name->HuboMan->Add(2,new HuboBase(EQPDAEDAEB));
	Name->HuboMan->Add(3,new HuboBase(EQPDAEDAEC));
/*
	Name->HuboMan->Add(0,new HuboBase(DAEDAEHQ));
	Name->HuboMan->Add(1,new HuboBase(DAEDAEA));
	Name->HuboMan->Add(2,new HuboBase(DAEDAEB));
	Name->HuboMan->Add(3,new HuboBase(DAEDAEC));
*/
	JKEdit *edit=new JKEdit(JKRect(120,60,250,80),ID_EDIT+2,Eqp24Data.Name24.Name,8,FALSE);
	AddControl(edit);

	edit=new JKEdit(JKRect(120,100,250,120),ID_EDIT+3,Eqp24Data.Name24.Number,11,FALSE);
	AddControl(edit);

	edit=new JKEdit(JKRect(120,140,250,160),ID_EDIT+4,Eqp24Data.Kind24.Name,14,FALSE);
	AddControl(edit);

	edit=new JKEdit(JKRect(120,180,250,200),ID_EDIT+5,Eqp24Data.Kind24.Number,19,FALSE);
	AddControl(edit);

	edit=new JKEdit(JKRect(320,20,450,40),ID_EDIT+6,0,5,FALSE);
	AddControl(edit);

	edit=new JKEdit(JKRect(320,60,450,80),ID_EDIT+7,0,5,FALSE);
	AddControl(edit);

	edit=new JKEdit(JKRect(320,100,450,120),ID_EDIT+8,0,5,FALSE);
	AddControl(edit);

	edit=new JKEdit(JKRect(320,140,450,160),ID_EDIT+9,0,5,FALSE);
	AddControl(edit);

	JKStatic *stc=new JKStatic(JKRect(20,20,100,40),0);
	stc->SetText("¦  ”");
	stc->BackColor=BackColor;
	stc->AdjustFlag=ADJ_YCENTER|ADJ_RIGHT;
	AddControl(stc);

	stc=new JKStatic(JKRect(20,60,100,80),0);
	stc->SetText("¬÷  ¡w");
	stc->BackColor=BackColor;
	stc->AdjustFlag=ADJ_YCENTER|ADJ_RIGHT;
	AddControl(stc);

	stc=new JKStatic(JKRect(20,100,100,120),0);
	stc->SetText("Š…  ¤å");
	stc->BackColor=BackColor;
	stc->AdjustFlag=ADJ_YCENTER|ADJ_RIGHT;
	AddControl(stc);

	stc=new JKStatic(JKRect(20,140,100,160),0);
	stc->SetText("Î‘  ¡w");
	stc->BackColor=BackColor;
	stc->AdjustFlag=ADJ_YCENTER|ADJ_RIGHT;
	AddControl(stc);

	stc=new JKStatic(JKRect(20,180,100,200),0);
	stc->SetText("¹A‰¡¤åÑ¡");
	stc->BackColor=BackColor;
	stc->AdjustFlag=ADJ_YCENTER|ADJ_RIGHT;
	AddControl(stc);

	stc=new JKStatic(JKRect(250,20,300,40),0);
	stc->SetText("·¥ˆa");
	stc->BackColor=BackColor;
	stc->AdjustFlag=ADJ_YCENTER|ADJ_RIGHT;
	AddControl(stc);

	stc=new JKStatic(JKRect(250,60,300,80),0);
	stc->SetText("A");
	stc->BackColor=BackColor;
	stc->AdjustFlag=ADJ_YCENTER|ADJ_RIGHT;
	AddControl(stc);

	stc=new JKStatic(JKRect(250,100,300,120),0);
	stc->SetText("B");
	stc->BackColor=BackColor;
	stc->AdjustFlag=ADJ_YCENTER|ADJ_RIGHT;
	AddControl(stc);

	stc=new JKStatic(JKRect(250,140,300,160),0);
	stc->SetText("C");
	stc->BackColor=BackColor;
	stc->AdjustFlag=ADJ_YCENTER|ADJ_RIGHT;
	AddControl(stc);

	JKFloatingButton *btn=new JKFloatingButton(JKRect(100,230,250,270),ID_BUTTON+1);
	btn->SetText("¸á¸w");
	stc->BackColor=BackColor;
	stc->AdjustFlag=ADJ_YCENTER|ADJ_RIGHT;
	AddControl(btn);

	btn=new JKFloatingButton(JKRect(300,230,450,270),ID_BUTTON+2);
	btn->SetText("Âá­¡");
	stc->BackColor=BackColor;
	stc->AdjustFlag=ADJ_YCENTER|ADJ_RIGHT;
	AddControl(btn);
}

void Equip24SellectDialog::EvShowWindow()
{
	JKDialog::EvShowWindow();
	JKControl *win=GetControl(ID_EDIT+1);
	if(win) win->SetFocus();
}


void Equip24SellectDialog::PaintClient(JKDC& dc,JKRect& paintrect)
{

}

void Equip24SellectDialog::EvButtonClick(WORD ctrlid)
{
	switch(ctrlid){
		case ID_BUTTON+1:
			PutRecord(Eqp24Data);
			SendMessage(ID_OK);
			break;
		case ID_BUTTON+2:
			SendMessage(ID_CANCEL);
			break;
	}
}

void Equip24SellectDialog::EvEditFinish(WORD ctrlid)
{
	JKEdit *edit;
	switch(ctrlid){
		case ID_EDIT+6:
			edit=(JKEdit *)GetControl(ID_EDIT+6);
			if(edit) Eqp24Data.Kind24.Inga=atoi(edit->GetText());
			break;
		case ID_EDIT+7:
			edit=(JKEdit *)GetControl(ID_EDIT+7);
			if(edit) Eqp24Data.Kind24.A=atoi(edit->GetText());
			break;
		case ID_EDIT+8:
			edit=(JKEdit *)GetControl(ID_EDIT+8);
				if(edit) Eqp24Data.Kind24.B=atoi(edit->GetText());
			break;
		case ID_EDIT+9:
			edit=(JKEdit *)GetControl(ID_EDIT+9);
			if(edit) Eqp24Data.Kind24.C=atoi(edit->GetText());
			break;
	}
}

//void Equip24SellectDialog::EvEditCancel(WORD /*ctrlid*/){}
//void Equip24SellectDialog::EvEditSetText(WORD /*ctrlid*/){


void Equip24SellectDialog::PutRecord(EQP24& eqp24d)
{
	Eq24DataMan.PutRecord(eqp24d,0x0007);
}

void Equip24SellectDialog::ClearData()
{
	setmem(&Eqp24Data,sizeof(Eqp24Data),'\0');
}

void Equip24SellectDialog::SetValue()
{
	RPTR recno=0;
	char buf[6];
	JKEdit *edit;

	Eq24DataMan.Eqp24Kind.GetRecord(Modify);
	recno=Eq24DataMan.Eqp24Kind.KindRecord.NameIndex;
	Eq24DataMan.Eqp24Name.GetRecord(recno);

	edit=(JKEdit *)GetControl(ID_EDIT+1);
	if(edit) edit->SetText(Eqp24Data.Name24.Attached);
	edit=(JKEdit *)GetControl(ID_EDIT+2);
	if(edit) edit->SetText(Eqp24Data.Name24.Name);
	edit=(JKEdit *)GetControl(ID_EDIT+3);
	if(edit) edit->SetText(Eqp24Data.Name24.Number);
	edit=(JKEdit *)GetControl(ID_EDIT+4);
	if(edit) edit->SetText(Eqp24Data.Kind24.Name);
	edit=(JKEdit *)GetControl(ID_EDIT+5);
	if(edit) edit->SetText(Eqp24Data.Kind24.Number);
	edit=(JKEdit *)GetControl(ID_EDIT+6);
	if(edit) {
		sprintf(buf,"%u",Eqp24Data.Kind24.Inga);
		edit->SetText(buf);
	}
	edit=(JKEdit *)GetControl(ID_EDIT+7);
	if(edit) {
		sprintf(buf,"%u",Eqp24Data.Kind24.A);
		edit->SetText(buf);
	}
	edit=(JKEdit *)GetControl(ID_EDIT+8);
	if(edit) {
		sprintf(buf,"%u",Eqp24Data.Kind24.B);
		edit->SetText(buf);
	}
	edit=(JKEdit *)GetControl(ID_EDIT+9);
	if(edit) {
		sprintf(buf,"%u",Eqp24Data.Kind24.C);
		edit->SetText(buf);
	}
}

