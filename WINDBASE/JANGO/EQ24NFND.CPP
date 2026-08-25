#include "eq24nfnd.h"
#include "combobox.h"
#include "jkedit.h"
#include "scrllist.h"
#include "jkstatic.h"
#include "fltbttn.h"
#include <mem.h>
#include <stdlib.h>
#include <string.h>

Equip24NameFind::Equip24NameFind(NAME24& eqp24name,WORD& compkind,char *text,WORD ctrlid,JKControl *parent):
	JKDialog(JKRect(300,200,600,450),parent,ctrlid),Eqp24Name(eqp24name),CompKind(compkind)
{
	SetText(text);
	//ClearData();
}

Equip24NameFind::~Equip24NameFind()
{
}

void Equip24NameFind::InitWindow()
{
	JKDialog::InitWindow();
}

void Equip24NameFind::SetupWindow()
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

	JKEdit *edit=new JKEdit(JKRect(80,20,230,40),ID_EDIT+1,Eqp24Name.Name,8,FALSE);
	AddControl(edit);

	edit=new JKEdit(JKRect(80,70,230,90),ID_EDIT+2,Eqp24Name.Number,11,FALSE);
	AddControl(edit);

	JKFloatingButton *fbtn=new JKFloatingButton(JKRect(30,160,130,200),ID_BUTTON+1);
  fbtn->SetText("ÑÂ·¥");
	AddControl(fbtn);

	fbtn=new JKFloatingButton(JKRect(160,160,260,200),ID_BUTTON+2);
  fbtn->SetText("Âá­¡");
	AddControl(fbtn);
}

void Equip24NameFind::EvShowWindow()
{
	JKDialog::EvShowWindow();
	JKControl *win=GetControl(ID_EDIT+1);
	if(win) win->SetFocus();
}

void Equip24NameFind::EvButtonClick(WORD ctrlid)
{
	switch(ctrlid){
		case ID_BUTTON+1:
			SendMessage(ID_OK);
			break;
		case ID_BUTTON+2:
			SendMessage(ID_CANCEL);
			break;
	}
}

void Equip24NameFind::EvEditFinish(WORD ctrlid)
{
	JKEdit *edit;

	switch(ctrlid){
		case ID_EDIT+1:
			edit=(JKEdit *)GetControl(ID_EDIT+1);
			if(edit){
				if(strlen(edit->GetText())>0){
					CompKind|=0x0004;
				}
				else CompKind&=~0x0004;
			}
			break;
		case ID_EDIT+2:
			edit=(JKEdit *)GetControl(ID_EDIT+2);
			if(edit){
				if(strlen(edit->GetText())>0){
					CompKind|=0x0008;
				}
				else CompKind&=~0x0008;
			}
			break;
	}

}

void Equip24NameFind::ClearData()
{
	setmem(&Eqp24Name,sizeof(NAME24),'\0');
}
/*
void Equip24NameFind::SetValue()
{
}
*/
void Equip24NameFind::EvDeadKeyDown(uint16 key,WORD /*modifier*/)
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

