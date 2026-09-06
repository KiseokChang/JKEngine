#include "eqp24cfg.h"
#include "eq24ipkd.h"
#include "combobox.h"
#include "jkedit.h"
#include "scrllist.h"
#include "jkstatic.h"
#include "fltbttn.h"
#include <mem.h>
#include <stdlib.h>
#include <string.h>

Equip24Config::Equip24Config(Equip24Kind& eqp24kind,RPTR sellect,char *text,WORD ctrlid,JKControl *parent):
	JKDialog(JKRect(70,150,930,630),parent,ctrlid),Eqp24Kind(eqp24kind),Sellect(sellect)
{
	SetText(text);
}
Equip24Config::~Equip24Config()
{
	Eqp24Kind.ChangeFileName(EQP24KIND);
}

void Equip24Config::InitWindow()
{
	JKDialog::InitWindow();
	InitFile();
	InitListBox();
}

void Equip24Config::SetupWindow()
{
	JKStatic *stc=new JKStatic(JKRect(10,5,660,25),0);
	stc->BackColor=Brown;
	stc->TextColor=White;
	stc->SetText(" ®…¤å  Î‘¡w           ¸‰¡¤åÑ¡              ·¥ˆa   ¥¡·A    A‹s    B‹s    C‹s");
	stc->AdjustFlag=ADJ_YCENTER;
	AddControl(stc);

	ScrollListBox *listbox=new ScrollListBox(JKRect(10,28,660,430),ID_LISTBOX+1);
	AddControl(listbox);

	JKFloatingButton *fbtn=new JKFloatingButton(JKRect(690,180,830,220),ID_BUTTON+1);
	fbtn->SetText("·³  b");
	fbtn->Depth=7;
	AddControl(fbtn);

	fbtn=new JKFloatingButton(JKRect(690,250,830,290),ID_BUTTON+2);
	fbtn->SetText("®  ¸÷");
	fbtn->Depth=7;
	AddControl(fbtn);

	fbtn=new JKFloatingButton(JKRect(690,320,830,360),ID_BUTTON+3);
	fbtn->SetText("¬b  ¹A");
	fbtn->Depth=7;
	AddControl(fbtn);

	fbtn=new JKFloatingButton(JKRect(690,390,830,430),ID_BUTTON+4);
	fbtn->SetText("aˆa‹¡");
	fbtn->Depth=7;
	AddControl(fbtn);
}

void Equip24Config::EvShowWindow()
{
	JKDialog::EvShowWindow();
	JKControl *win=GetControl(ID_BUTTON+1);
	if(win) win->SetFocus();
}

void Equip24Config::EvButtonClick(WORD ctrlid)
{
	HuboBase *hubo;
	Eqp24InputKindDialog *input;
	ScrollListBox *listbox;

	switch(ctrlid) {
		case ID_BUTTON+1:
			input=new Eqp24InputKindDialog(Eqp24Kind);
			if(input){
				input->Run();
				delete input;
				InitListBox();
			}
		break;
		case ID_BUTTON+2:
			listbox=(ScrollListBox *)GetControl(ID_LISTBOX+1);
			if(listbox) {
				if(listbox->CurrentIndex!=-1) {
					hubo=listbox->GetHuboBase(listbox->CurrentIndex);
					if(hubo) {
						input=new Eqp24InputKindDialog(Eqp24Kind,hubo->Number);
						if(input){
							input->Run();
							delete input;
							InitListBox();
						}
					}
				}
			}
		break;
		case ID_BUTTON+3:
			listbox=(ScrollListBox *)GetControl(ID_LISTBOX+1);
			if(listbox) {
				if(listbox->CurrentIndex!=-1) {
					hubo=listbox->GetHuboBase(listbox->CurrentIndex);
					if(hubo) {
						Eqp24Kind.DeleteRecord(hubo->Number);
						InitListBox();
					}
				}
			}
		break;
		case ID_BUTTON+4:
			SendMessage(ID_OK);
		break;
	}
}

void Equip24Config::EvDeadKeyDown(uint16 key,WORD /*modifier*/)
{
	WORD ctrlid;
	JKControl* ctrl;
	WORD LastCtrlID = ID_BUTTON+4;
	if(key==PGDNKEY){
		if(FindSubWindow(MsgFilter->FocusWinID,ctrlid,TRUE)&&ctrlid){
			if(ctrlid>ID_BUTTON+1&&ctrlid<LastCtrlID) ctrlid++;
			else if(ctrlid==LastCtrlID) ctrlid=ID_BUTTON+1;
			else if(ctrlid==ID_BUTTON+1) ctrlid++;
			else ctrlid=ID_BUTTON+1;

			ctrl=GetControl(ctrlid);
			if(ctrl) ctrl->SetFocus();
		}
	}
	else if(key==PGUPKEY){
		if(FindSubWindow(MsgFilter->FocusWinID,ctrlid,TRUE)&&ctrlid){
			if(ctrlid>ID_BUTTON+1&&ctrlid<LastCtrlID) ctrlid--;
			else if(ctrlid==LastCtrlID)	ctrlid--;
			else if(ctrlid==ID_BUTTON+1)	ctrlid=LastCtrlID;
			else ctrlid=ID_BUTTON+1;

			ctrl=GetControl(ctrlid);
			if(ctrl) ctrl->SetFocus();
		}
	}
}

void Equip24Config::InitFile()
{
	if(Sellect==0) Eqp24Kind.ChangeFileName(EQP24001);
	else if(Sellect==1) Eqp24Kind.ChangeFileName(EQP24002);
	else if(Sellect==2) Eqp24Kind.ChangeFileName(EQP24003);
}

void Equip24Config::InitListBox()
{
	RPTR index=1;
	char buf[100];
	WORD boyou=0;
	int16 realcount=0;

	ScrollListBox *listbox=(ScrollListBox *)GetControl(ID_LISTBOX+1);
	if(listbox) {
		listbox->DestroyAll();
		while(index<Eqp24Kind.DFileMan.FileHeader[Eqp24Kind.FilePoint].Record_Count){
			if(Eqp24Kind.GetRecord(index)){
				boyou=Eqp24Kind.KindRecord.A+Eqp24Kind.KindRecord.B+Eqp24Kind.KindRecord.C;
				sprintf(buf,"%5u %-14s  %-19s %5u  %5u  %5u  %5u  %5u",Eqp24Kind.KindRecord.Order,Eqp24Kind.KindRecord.Name,Eqp24Kind.KindRecord.Number,Eqp24Kind.KindRecord.Inga,boyou,Eqp24Kind.KindRecord.A,Eqp24Kind.KindRecord.B,Eqp24Kind.KindRecord.C);
				listbox->Add(realcount,new HuboBase(buf,(WORD)index));
				realcount++;
			}
			index++;
		}
		listbox->Add(realcount,new HuboBase(" *****  µa‹¡ˆa {·³“¡”a  *****"));
		listbox->RedrawWindow();
	}
}

