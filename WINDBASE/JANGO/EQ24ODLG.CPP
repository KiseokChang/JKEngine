#include "eq24odlg.h"
#include "eqpprtmn.h"
#include "jkstatic.h"
#include "fltbttn.h"
#include <mem.h>
#include <stdlib.h>
#include <string.h>

Eqp24OtherDialog::Eqp24OtherDialog(Equip24DataManager& eqp24dm,int16 sel,char *budae,WORD ctrlid,JKControl *parent):
	JKDialog(JKRect(25,120,950,670),parent,ctrlid),Eqp24DataMan(eqp24dm),Sellect(sel),BudaeDiscern(budae)
{
  if(!Sellect) SetText("¤es ÑeÑ×");
  else SetText("­¡¡¡/¬b¹A ÑeÑ×");

	if(!Sellect){
		Eqp24DataMan.Eqp24Kind.ChangeFileName(EQ24RKIND);
		Eqp24DataMan.Eqp24Name.ChangeFileName(EQ24RNAME);
	}
	else {
		Eqp24DataMan.Eqp24Kind.ChangeFileName(EQ24SKIND);
		Eqp24DataMan.Eqp24Name.ChangeFileName(EQ24SNAME);
	}
}

Eqp24OtherDialog::~Eqp24OtherDialog()
{
	Eqp24DataMan.Eqp24Kind.ChangeFileName(EQP24KIND);
	Eqp24DataMan.Eqp24Name.ChangeFileName(EQP24NAME);
}

void Eqp24OtherDialog::InitWindow()
{
	JKDialog::InitWindow();
	InitListBox();
}

void Eqp24OtherDialog::SetupWindow()
{
	JKStatic *stc=new JKStatic(JKRect(10,5,750,30),ID_STATIC+1);
	stc->BackColor=Brown;
	stc->TextColor=White;
	stc->SetText(" ®…¤å  Î‘¡w           ¸‰¡¤åÑ¡              ·¥ˆa   ¥¡·A    A‹s    B‹s    C‹s   ·©¸a");
	stc->AdjustFlag=ADJ_YCENTER;
	AddControl(stc);

	ScrollListBox *scrllbox=new ScrollListBox(JKRect(10,32,750,500),ID_LISTBOX+1);
	AddControl(scrllbox);

	JKFloatingButton *btn=new JKFloatingButton(JKRect(770,400,900,440),ID_BUTTON+1);
	btn->SetText("Â‰b");
	AddControl(btn);

	btn=new JKFloatingButton(JKRect(770,460,900,500),ID_BUTTON+2);
	btn->SetText("aˆa‹¡");
	AddControl(btn);
}

void Eqp24OtherDialog::EvShowWindow()
{
	JKDialog::EvShowWindow();
	JKControl *win=GetControl(ID_BUTTON+1);
	if(win) win->SetFocus();
}

void Eqp24OtherDialog::EvButtonClick(WORD ctrlid)
{
	switch(ctrlid){
		case ID_BUTTON+1:
			Print();
			break;
		case ID_BUTTON+2:
			SendMessage(ID_OK);
			break;
	}
}

void Eqp24OtherDialog::InitListBox()
{
	RPTR index=1;
	int16  realcount=0;
	ScrollListBox *scrllbox=(ScrollListBox *)GetControl(ID_LISTBOX+1);
	if(scrllbox) {
		while(index<Eqp24DataMan.Eqp24Kind.DFileMan.FileHeader[Eqp24DataMan.Eqp24Kind.FilePoint].Record_Count) {
			if(Eqp24DataMan.Eqp24Kind.GetRecord(index)){
				Eqp24DataMan.Eqp24Name.GetRecord(Eqp24DataMan.Eqp24Kind.KindRecord.NameIndex);
				if(!strcmp(Eqp24DataMan.Eqp24Name.NameRecord.Attached,BudaeDiscern)||!strcmp(BudaeName,BudaeDiscern)){
					MakeString(Eqp24DataMan.Eqp24Kind.KindRecord,index,realcount);
					realcount++;
				}
			}
			index++;
		}
		scrllbox->Add(realcount,new HuboBase(" *****  µa‹¡ˆa {·³“¡”a  *****"));
	}
}

void Eqp24OtherDialog::MakeString(KIND24& kind24,RPTR index,int16 realcount)
{
	char buf[100];
	WORD boyou=kind24.A+kind24.B+kind24.C;
	sprintf(buf,"%5u %-14s  %-19s %5u  %5u  %5u  %5u  %5u  %-10s",kind24.Order,kind24.Name,kind24.Number,kind24.Inga,boyou,kind24.A,kind24.B,kind24.C,kind24.Date);
	ScrollListBox *scrllbox=(ScrollListBox *)GetControl(ID_LISTBOX+1);
	if(scrllbox) {
		scrllbox->Add(realcount,new HuboBase(buf,(WORD)index));
	}
}

void Eqp24OtherDialog::Print()
{
	int16 count=0;
	HuboBase *hubo;
	Equip24PrintManager prt;

	HeadForm(1);

	ScrollListBox *scrllbox=(ScrollListBox *)GetControl(ID_LISTBOX+1);
	if(scrllbox){
		while(count<scrllbox->HuboManager::NodeCount-1){
			if(count&&count%50==0){
				prt.PPutnch('=',90);
				prt.FormFeed();
				HeadForm(count/50+1);
			}
			hubo=scrllbox->HuboManager::GetNode(count);
			prt.PPuts(hubo->TextBuff);
			prt.LineFeed();
			count++;
		}
	}
	prt.PPutnch('=',90);
	prt.LineFeed();
	prt.FormFeed();
}

void Eqp24OtherDialog::HeadForm(int16 capt)
{
	char buf[70];
	date Date;
HuboBase *hubo;
	Equip24PrintManager prt;

	getdate(&Date);
	sprintf(buf,"Â‰b·© : %4d.%2d.%2d               PAGE : -%d-",Date.da_year,Date.da_mon,Date.da_day,capt);

	if(!Sellect) prt.PPuts("                                    < 2.4¹·  ¤es ÑeÑ×  >");
	else prt.PPuts("                                     < 2.4¹·  ­¡¡¡/¬b¹A ÑeÑ×  >");

	prt.LineFeed();
	prt.PPutnch(' ',45);
	prt.PPuts(buf);
	prt.LineFeed();
//	prt.LineFeed();

	prt.PPutnch('=',90);
	prt.LineFeed();

	JKControl *ctrl=GetControl(ID_STATIC+1);
	if(ctrl){
		prt.PPuts(ctrl->GetText());
		prt.LineFeed();
	}
	prt.PPutnch('=',90);
	prt.LineFeed();
}

void Eqp24OtherDialog::EvDeadKeyDown(uint16 key,WORD /*modifier*/)
{
	WORD ctrlid;
	JKControl* ctrl;
	WORD LastCtrlID = ID_BUTTON+2;
	if(key==PGDNKEY){
		if(FindSubWindow(MsgFilter->FocusWinID,ctrlid,TRUE)&&ctrlid){
			if(ctrlid>=ID_BUTTON+1&&ctrlid<LastCtrlID) ctrlid++;
			else if(ctrlid==LastCtrlID) ctrlid=ID_BUTTON+1;
			else if(ctrlid==ID_BUTTON+1) ctrlid=ID_BUTTON+2;
			else ctrlid=ID_BUTTON+1;

			ctrl=GetControl(ctrlid);
			if(ctrl) ctrl->SetFocus();
		}
	}
	else if(key==PGUPKEY){
		if(FindSubWindow(MsgFilter->FocusWinID,ctrlid,TRUE)&&ctrlid){
			if(ctrlid>ID_BUTTON+1&&ctrlid<=LastCtrlID) ctrlid--;
			else if(ctrlid==ID_BUTTON+1)	ctrlid=LastCtrlID;
			else if(ctrlid==LastCtrlID)	ctrlid=LastCtrlID;
			else ctrlid=LastCtrlID;

			ctrl=GetControl(ctrlid);
			if(ctrl) ctrl->SetFocus();
		}
	}
}

