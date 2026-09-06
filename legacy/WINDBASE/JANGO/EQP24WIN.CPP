#include "eqp24win.h"
#include "eqp24glb.h"
#include "eqnmlist.h"
#include "eqkdlist.h"
#include "eq24idlg.h"
#include "eq24pdlg.h"
#include "eq24somo.h"
#include "eqp24cfg.h"
#include "eq24ipfa.h"
#include "eq24nfnd.h"
#include "eqpprtmn.h"
#include "eq24odlg.h"
#include "jkstatic.h"
#include "jkbutton.h"
#include "fltbttn.h"
#include "joclock.h"
#include "sollucvt.h"

#define MessageCount  16

static char *HelpMessage[MessageCount]={
  "ˆ·¥»¡‹s–E Î‘¡¢·i ‰ÅŸ¡Ði ® ·¶¯s“¡”a.",
  "ˆ·¥»¡‹sÎ‘ ¶AµA Í¡”/º—” ‰ÅŸ¡ Î‘¡¢µA ”Ðe ‰ÅŸ¡Ÿi Ði ® ·¶¯s“¡”a",
  "ˆ·¥¢‰¸aµÁ Í¡”/º—” ‰ÅŸ¡¢‰¸a ¸åÁAŸi ‰ÅŸ¡ Ði ® ·¶¯s“¡”a",
  "¦”¡wµA ˜aŸe Î‘¡¢Ðw·i ¬å¸÷Ðaµa ¥© ® ·¶¯s“¡”a",
  "¸å·³,¸åÂ‰,Ò­·,¸åµb¯¡ »¡‹sÎ‘· ‹¡¥¥¬aÐw·i ¸a•·‰¬eÐº‘,... ·¥¶¥·i ¬é¸÷Ða¯³¯¡¶a",
  "¥wb·¡•·¯¡(¸å·³,¸åÂ‰,Ò­·,¸åµb) Ï©¶aÐe ¢‰Î‘—i·i ·³bÐs“¡”a",
  "ˆ·¥ ™¡“e ·©¤e ¢‰¸aŸi ·³bÐs“¡”a",
  "·³b–A´á ·¶“e ¶w·i ®¸÷Ði ® ·¶¯s“¡”a",
  "Ñe¹A ·³b–A´á ·¶“e ¢‰Î‘·i ­¡¡¡/¬b¹A ¯¡Ç© ® ·¶¯s“¡”a(­¡¡¡ ÑÁ·©µA ¸á¸w)",
  "Ñe¹A ·³b–A´á ·¶“e ¢‰Î‘·i ¤es¯¡Ç© ® ·¶¯s“¡”a(¤es ÑÁ·©µA ¸á¸w)",
  "¬÷¡w‰Á Š…¤å·a¡ ˆ·¥ »¡‹sÎ‘·i ˆñ¬‚Ðs“¡”a",
  "·³b–E ¢‰¸aº—µA¬á C—w‹sµA Ð”wÐa“e ˆõ·i Àx´aº““¡”a / Ñe¸· ¬wÑ×(ˆ·¥/·©¤e¢‰¸a)Ÿi ÑÂ·¥Ða­A¶a ",
  "¤esÐe ¢‰Î‘·i ¥© ® ·¶¯s“¡”a",
  "­¡¡¡/¬b¹AÐe ¢‰Î‘·i  ¥© ® ·¶¯s“¡”a",
  "Ñe¸ ÑÁ¡e¬wÑ×·i  ÏaŸ¥Ëa¡ Â‰bÐs“¡”a, !!!! ÏaŸ¥ËaŸi º…§¡Ða­A¶a!!!!",
  "2,4¹· ‰ÅŸ¡ Ïa¡‹aœ‘·i ¹·žaÐs“¡”a"
};

Equip24Window::Equip24Window(WORD ctrlid):
	JKDialog(JKRect(0,0,getmaxx(),getmaxy()),0,ctrlid),Eqp24DMan(),
	C_Value(FALSE),ShowNumber(100)
{
	WinForm=0x03;
	Depth=1;
	BorderSize=2;
	BackColor=LtGray;
	WinAttr|= WA_FIRSTSHOW | WA_CHANGEPLACE;
	SetText("2.4¹· ‰ÅŸ¡ - Copyright 2 C/A 1996.1.31(Ver 2.0) - ");
	strcpy(ObjectDiscern,"ˆ·¥");
	strcpy(BudaeDiscern,EQPDAEDAEHQ);
}

Equip24Window::~Equip24Window()
{
	WriteConfig();
}

void Equip24Window::InitWindow()
{
	JKDialog::InitWindow();
	ChangeDate();
	InitConfig();
	ChangeTitleStat();
	SendMessage(REDRAWLISTBOX);
}

void Equip24Window::SetupWindow()
{
	JKButton *btn=new JKButton(JKRect(1,1,1020,30),0);
	btn->BackColor=Blue;
	btn->SetEnable(FALSE);
	btn->Depth=0;
	AddControl(btn);

	JKFloatingButton *fbtn=new JKFloatingButton(JKRect(1,33,1025,103),0);
	fbtn->BackColor=Cyan;
	fbtn->SetEnable(FALSE);
	fbtn->Depth=15;
	AddControl(fbtn);

	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	JKStatic *stc=new JOClock(JKRect(930,1,1000,30),0);
	stc->BackColor=Blue;
	stc->TextColor=Yellow;
	stc->AdjustFlag=ADJ_XYCENTER;
	AddControl(stc);

	stc=new JKStatic(JKRect(5,115,220,137),0);
	stc->BackColor=Brown;
	stc->TextColor=White;
	stc->SetText("  ¬÷¡w        Š…  ¤å");
	stc->AdjustFlag=ADJ_YCENTER;
	AddControl(stc);

	stc=new JKStatic(JKRect(225,115,870,137),ID_STATIC+99);
	stc->BackColor=Brown;
	stc->TextColor=White;
  stc->SetText(" ®…¤å  Î‘¡w           ¸‰¡¤åÑ¡              ·¥ˆa   ¥¡·A    A‹s    B‹s    C‹s");
	stc->AdjustFlag=ADJ_YCENTER;
	AddControl(stc);

	stc=new JKStatic(JKRect(20,1,100,30),0);
	stc->BackColor=Blue;
	stc->TextColor=White;
	stc->SetText(EQPTITLENAME);
	stc->AdjustFlag=ADJ_XYCENTER;
	AddControl(stc);

	stc=new JKStatic(JKRect(650,1,750,30),ID_STATIC+1);
	stc->BackColor=Blue;
	stc->TextColor=White;
	stc->AdjustFlag=ADJ_XYCENTER;
	AddControl(stc);

	stc=new JKStatic(JKRect(780,1,900,30),ID_STATIC+2);
	stc->BackColor=Blue;
	stc->TextColor=White;
	stc->AdjustFlag=ADJ_XYCENTER;
	AddControl(stc);

	stc=new JKStatic(JKRect(750,43,1000,80),ID_STATIC+3);
	stc->BackColor=Blue;
	stc->TextColor=White;
  stc->SetText("¦”:          ¬wÑ×:      ");
	stc->AdjustFlag=ADJ_XYCENTER;
	AddControl(stc);

	stc=new JKStatic(JKRect(2,710,1021,736),ID_STATIC+4);
	stc->BackColor=Gray;
	stc->AdjustFlag=ADJ_YCENTER;
	AddControl(stc);

	btn=new JKButton(JKRect(20,43,120,80),ID_BUTTON+1);
	btn->SetText("ˆ·¥ »¡‹sÎ‘");
	btn->BackColor=LtGray;
	btn->TextColor=LtMagenta;
	AddControl(btn);

	btn=new JKButton(JKRect(140,43,240,80),ID_BUTTON+2);
	btn->SetText("·©¤e¢‰¸a");
	btn->BackColor=LtGray;
	btn->TextColor=LtMagenta;
	AddControl(btn);

	btn=new JKButton(JKRect(260,43,360,80),ID_BUTTON+3);
	btn->SetText("¸åÁAÑeÑ×");
	btn->BackColor=LtGray;
	btn->TextColor=LtMagenta;
	AddControl(btn);

	btn=new JKButton(JKRect(380,43,480,80),ID_BUTTON+4);
	btn->SetText("¦”¬å¸÷");
	btn->BackColor=LtGray;
	btn->TextColor=LtMagenta;
	AddControl(btn);

	btn=new JKButton(JKRect(500,43,600,80),ID_BUTTON+5);
	btn->SetText("¥wb·¡•·");
	btn->BackColor=LtGray;
	btn->TextColor=LtMagenta;
	AddControl(btn);

	btn=new JKButton(JKRect(620,43,720,80),ID_BUTTON+6);
	btn->SetText("ÑÅ‰w¬é¸÷");
	btn->BackColor=LtGray;
	btn->TextColor=LtMagenta;
	AddControl(btn);

	fbtn=new JKFloatingButton(JKRect(880,210,1010,250),ID_BUTTON+7);
	fbtn->SetText("·³    b");
	fbtn->Depth=10;
	AddControl(fbtn);

	fbtn=new JKFloatingButton(JKRect(880,260,1010,300),ID_BUTTON+8);
	fbtn->SetText("®    ¸÷");
	fbtn->Depth=10;
	AddControl(fbtn);

	fbtn=new JKFloatingButton(JKRect(880,310,1010,350),ID_BUTTON+9);
	fbtn->SetText("­¡¡¡/¬b¹A");
	fbtn->Depth=10;
	AddControl(fbtn);

	fbtn=new JKFloatingButton(JKRect(880,360,1010,400),ID_BUTTON+10);
	fbtn->SetText("¤e    s");
	fbtn->Depth=10;
	AddControl(fbtn);

	fbtn=new JKFloatingButton(JKRect(880,410,1010,450),ID_BUTTON+11);
	fbtn->SetText("ˆ·¥ˆñ¬‚");
	fbtn->Depth=10;
	AddControl(fbtn);

	fbtn=new JKFloatingButton(JKRect(880,460,1010,500),ID_BUTTON+12);
	fbtn->SetText("C—w‹s ˆñ¬‚");
	fbtn->Depth=10;
	AddControl(fbtn);

	fbtn=new JKFloatingButton(JKRect(880,510,1010,550),ID_BUTTON+13);
	fbtn->SetText("¤es ÑeÑ×");
	fbtn->Depth=10;
	AddControl(fbtn);

	fbtn=new JKFloatingButton(JKRect(880,560,1010,600),ID_BUTTON+14);
	fbtn->SetText("­¡¡¡/¬b¹AÑeÑ×");
	fbtn->Depth=10;
	AddControl(fbtn);

	fbtn=new JKFloatingButton(JKRect(880,610,1010,650),ID_BUTTON+15);
	fbtn->SetText("Â‰       b");
	fbtn->Depth=10;
	AddControl(fbtn);

	fbtn=new JKFloatingButton(JKRect(880,660,1010,700),ID_BUTTON+16);
	fbtn->SetText("¹·       ža");
	fbtn->Depth=10;
	AddControl(fbtn);

	EquipNameListBox *list=new EquipNameListBox(JKRect(5,140,220,700),Eqp24DMan.Eqp24Name,BudaeDiscern,ObjectDiscern,ID_LISTBOX+1);
	AddControl(list);

	EquipKindListBox *list1=new EquipKindListBox(JKRect(225,140,870,700),Eqp24DMan.Eqp24Kind,Eqp24DMan.Eqp24Name,ObjectDiscern,ID_LISTBOX+2);
	AddControl(list1);

	JKListBox *listbox=new JKListBox(JKRect(380,83,480,170),ID_LISTBOX+3);
	listbox->WinAttr&=~BYTE(WA_FIRSTSHOW);
	AddControl(listbox);
	listbox->Add(0,(new HuboBase(BudaeName,0)));
	listbox->Add(1,(new HuboBase(EQPDAEDAEHQ,1)));
	listbox->Add(2,(new HuboBase(EQPDAEDAEA,2)));
	listbox->Add(3,(new HuboBase(EQPDAEDAEB,3)));
	listbox->Add(4,(new HuboBase(EQPDAEDAEC,4)));

	listbox=new JKListBox(JKRect(500,83,600,140),ID_LISTBOX+4);
	listbox->WinAttr&=~BYTE(WA_FIRSTSHOW);
	AddControl(listbox);
	listbox->Add(0,(new HuboBase("¸å·³",0)));
	listbox->Add(1,(new HuboBase("¸åÂ‰/Ò­·",1)));
	listbox->Add(2,(new HuboBase("¸åµb",2)));

	listbox=new JKListBox(JKRect(620,83,720,140),ID_LISTBOX+5);
	listbox->WinAttr&=~BYTE(WA_FIRSTSHOW);
	AddControl(listbox);
	listbox->Add(0,(new HuboBase("‹¡¥¥ ¥¡‹sÎ‘",0)));
	listbox->Add(1,(new HuboBase("¦” »¡‹sÎ‘",1)));
	listbox->Add(2,(new HuboBase("¸åµb ¤esÎ‘",2)));
}

void Equip24Window::PaintClient(JKDC& dc,JKRect& /*paintrect*/)
{
	dc.PutIMG("log100.img",JKPoint(890,110),JKRect(880,110,1010,260),COPY_PUT);
}

void Equip24Window::EvButtonClick(WORD ctrlid)
{
	JKControl *ctrl;
	HuboBase *hubo;
	EquipKindListBox *kindlist;
	EquipNameListBox *namelist;
	Eqp24OtherDialog *othdlg;

	switch(ctrlid) {
		case ID_BUTTON+1:
			ListBoxHide();
			strcpy(ObjectDiscern,"ˆ·¥");
			SendMessage(CHANGETITLE);
			SendMessage(REDRAWLISTBOX);
			break;
		case ID_BUTTON+2:
			ListBoxHide();
			strcpy(ObjectDiscern,"·©¤e");
			SendMessage(CHANGETITLE);
			SendMessage(REDRAWLISTBOX);
			break;
		case ID_BUTTON+3:
			ListBoxHide();
			strcpy(ObjectDiscern,"¸åÁA");
			SendMessage(CHANGETITLE);
			kindlist=(EquipKindListBox *)GetControl(ID_LISTBOX+2);
			if(kindlist) {
			kindlist->InitTotal();
			kindlist->RedrawWindow();
			}
			namelist=(EquipNameListBox *)GetControl(ID_LISTBOX+1);
			if(namelist){
				namelist->DestroyAll();
				namelist->RedrawWindow();
			}
			break;
		case ID_BUTTON+4:
			ListBoxHide();
			ctrl=GetControl(ID_LISTBOX+3);
			if(ctrl) ctrl->ShowWindow();
			break;
		case ID_BUTTON+5:
			ListBoxHide();
			if(strcmp(BudaeDiscern,BudaeName)) {
				ctrl=GetControl(ID_LISTBOX+4);
				if(ctrl) ctrl->ShowWindow();
			}
			break;
		case ID_BUTTON+6:
			ListBoxHide();
			ctrl=GetControl(ID_LISTBOX+5);
			if(ctrl) ctrl->ShowWindow();
			break;
		case ID_BUTTON+7:
			ListBoxHide();
			if(strcmp(BudaeDiscern,BudaeName))	SellectInput(0);
			break;
		case ID_BUTTON+8:
			ListBoxHide();
			kindlist=(EquipKindListBox *)GetControl(ID_LISTBOX+2);
			if(kindlist){
				if(kindlist->CurrentIndex!=-1) {
					hubo=kindlist->GetHuboBase(kindlist->CurrentIndex);
					if(hubo) {
						SellectInput(hubo->Number);
					}
				}
			}
			break;
		case ID_BUTTON+9:
			ListBoxHide();
			if(strcmp(ObjectDiscern,"¸åÁA")) {
				kindlist=(EquipKindListBox *)GetControl(ID_LISTBOX+2);
				if(kindlist){
					if(kindlist->CurrentIndex!=-1) {
						hubo=kindlist->GetHuboBase(kindlist->CurrentIndex);
						if(hubo) {
							Eqp24InputSomoDialog *somodlg=new Eqp24InputSomoDialog(Eqp24DMan,hubo->Number,TRUE);
							if(somodlg) {
								somodlg->Run();
								delete somodlg;
								SendMessage(SAVE_FINISH);
							}
						}
					}
				}
			}
			break;
		case ID_BUTTON+10:
			ListBoxHide();
			if(strcmp(ObjectDiscern,"¸åÁA")) {
				kindlist=(EquipKindListBox *)GetControl(ID_LISTBOX+2);
				if(kindlist){
					if(kindlist->CurrentIndex!=-1) {
						hubo=kindlist->GetHuboBase(kindlist->CurrentIndex);
						if(hubo) {
							Eqp24InputSomoDialog *somodlg=new Eqp24InputSomoDialog(Eqp24DMan,hubo->Number,FALSE);
							if(somodlg) {
								somodlg->Run();
								delete somodlg;
								SendMessage(SAVE_FINISH);
							}
						}
					}
				}
			}
			break;
		case ID_BUTTON+11:
			ListBoxHide();
			FindName("ˆ·¥ˆñ¬‚");
			break;
		case ID_BUTTON+12:
			ListBoxHide();
			C_Value=TRUE;
			FindCClass();
			break;
		case ID_BUTTON+13:
			ListBoxHide();
			othdlg=new Eqp24OtherDialog(Eqp24DMan,0,BudaeDiscern);
			if(othdlg){
				othdlg->Run();
				delete othdlg;
			}
			break;
		case ID_BUTTON+14:
			ListBoxHide();
			othdlg=new Eqp24OtherDialog(Eqp24DMan,1,BudaeDiscern);
			if(othdlg){
				othdlg->Run();
				delete othdlg;
			}
			break;
		case ID_BUTTON+15:
			ListBoxHide();
			Print();
			break;
		case ID_BUTTON+16:
			SendMessage(ID_OK);
			break;
	}
}

ProcessResult Equip24Window::RespondMessage(JKMSG& msg)
{
	EquipKindListBox *kindlist;
	EquipNameListBox *namelist;
	HuboBase *hubo;

	switch(msg.main) {
		case CLOCK_CHANGEDATE:
			ChangeDate();
			break;
		case SAVE_FINISH:
			namelist=(EquipNameListBox *)GetControl(ID_LISTBOX+1);
			if(namelist) {
				namelist->InitListBox();
				namelist->RedrawClient();
			}
			kindlist=(EquipKindListBox *)GetControl(ID_LISTBOX+2);
			if(kindlist) {
				kindlist->DestroyAll();
				kindlist->RedrawClient();
			}
			break;
		case DELETEMSG:
			kindlist=(EquipKindListBox *)GetControl(ID_LISTBOX+2);
			if(kindlist) {
				if(kindlist->CurrentIndex!=-1) {
					hubo=kindlist->GetHuboBase(kindlist->CurrentIndex);
					if(hubo) {
						Eqp24DMan.DeleteRecord(hubo->Number,2);
						kindlist->RedrawClient();
					}
				}
				kindlist->DestroyAll();
				kindlist->RedrawClient();
				namelist=(EquipNameListBox *)GetControl(ID_LISTBOX+1);
				if(namelist){
					namelist->InitListBox();
					namelist->RedrawWindow();
				}
			}
			else {
				namelist=(EquipNameListBox *)GetControl(ID_LISTBOX+1);
				if(namelist) {
					if(namelist->CurrentIndex!=-1L) {
						hubo=namelist->GetHuboBase(namelist->CurrentIndex);
						if(hubo) {
							Eqp24DMan.DeleteRecord(hubo->Number,1);
							namelist->InitListBox();
							namelist->RedrawWindow();
						}
					}
				}
			}
			break;
		case REDRAWLISTBOX:
			namelist=(EquipNameListBox *)GetControl(ID_LISTBOX+1);
			if(namelist){
				namelist->DestroyAll();
				namelist->InitListBox();
				namelist->RedrawWindow();
			}
			kindlist=(EquipKindListBox *)GetControl(ID_LISTBOX+2);
			if(kindlist){
				kindlist->DestroyAll();
				kindlist->RedrawWindow();
			}
			break;
		case CHANGETITLE:
			ChangeTitleStat();
			break;
	}
	return JKDialog::RespondMessage(msg);
}

void Equip24Window::EvLBTouchEntry(WORD ctrlid,uint16 index)
{
	HuboBase *hubo;
	EquipKindListBox *kindlist;
	EquipNameListBox *namelist;

	switch (ctrlid) {
		case ID_LISTBOX+1:
			if(!C_Value){
				namelist=(EquipNameListBox *)GetControl(ID_LISTBOX+1);
				if(namelist){
					hubo=namelist->GetHuboBase(index);
					if(hubo){
						kindlist=(EquipKindListBox *)GetControl(ID_LISTBOX+2);
						if(kindlist){
							if((int16)hubo->Number==-1) kindlist->DestroyAll();
							else kindlist->ShowString(hubo->Number);
							kindlist->RedrawClient();
						}
					}
				}
			}
			else if(C_Value){
				namelist=(EquipNameListBox *)GetControl(ID_LISTBOX+1);
				if(namelist){
					hubo=namelist->GetHuboBase(index);
					if(hubo){
						kindlist=(EquipKindListBox *)GetControl(ID_LISTBOX+2);
						if(kindlist){
							if((int16)hubo->Number==-1) kindlist->DestroyAll();
							else kindlist->ShowCKind(hubo->Number);
							kindlist->RedrawClient();
						}
					}
				}
			}
			break;
	}
}

void Equip24Window::EvNcOnlyMove(JKPoint& p)
{
	JKRect rect;
	JKListBox *listbox=(JKListBox *)GetControl(ID_LISTBOX+3);
	if(listbox) {
		rect=listbox->WinRect.ExpandBy(16);
		if(!rect.IsContain(p)) listbox->HideWindow();
	}
	listbox=(JKListBox *)GetControl(ID_LISTBOX+4);
	if(listbox) {
		rect=listbox->WinRect.ExpandBy(16);
		if(!rect.IsContain(p)) listbox->HideWindow();
	}
	listbox=(JKListBox *)GetControl(ID_LISTBOX+5);
	if(listbox) {
		rect=listbox->WinRect.ExpandBy(16);
		if(!rect.IsContain(p)) listbox->HideWindow();
	}
}

void Equip24Window::EvOnlyMove(JKPoint&)
{
	JKControl *ctrl=GetControl(ID_STATIC+4);
	if(ctrl&&ShowNumber!=99){
    ctrl->SetText("•¡¶£¡ ==> ¬a¶wº— ·¢…¬aÐw·¡ ¬—‹¡¡e ”¶á ‹±Ñw­AŸi Àx·a­A¶a (0363)442-6602");
		ctrl->RedrawWindow();
		ShowNumber=99;
	}
}

void Equip24Window::EvLBSelectEntry(WORD ctrlid,uint16 index)
{
	JKListBox *listbox;
	HuboBase *hubo;

	switch(ctrlid){
		case ID_LISTBOX+3:
			listbox=(JKListBox *)GetControl(ID_LISTBOX+3);
			if(listbox){
				hubo=listbox->GetHuboBase(index);
				if(hubo) {
					strcpy(BudaeDiscern,hubo->TextBuff);
					SendMessage(CHANGETITLE);
					listbox->HideWindow();
					SendMessage(REDRAWLISTBOX);
				}
			}
			break;
		case ID_LISTBOX+4:
			listbox=(JKListBox *)GetControl(ID_LISTBOX+4);
			if(listbox){
				listbox->EvHideWindow();
				hubo=listbox->GetHuboBase(index);
				if(hubo) {
					Equip24InputFA *dlg=new Equip24InputFA(Eqp24DMan,hubo->Number,hubo->TextBuff,BudaeDiscern);
					if(dlg) {
						WORD rtv=dlg->Run();
						if(rtv==ID_OK)SendMessage(REDRAWLISTBOX);
						delete dlg;
					}
				}
			}
			break;
		case ID_LISTBOX+5:
			listbox=(JKListBox *)GetControl(ID_LISTBOX+5);
			if(listbox){
				listbox->EvHideWindow();
				hubo=listbox->GetHuboBase(index);
				if(hubo) {
					Equip24Config *dlg=new Equip24Config(Eqp24DMan.Eqp24Kind,hubo->Number,hubo->TextBuff);
					if(dlg) {
						dlg->Run();
						delete dlg;
					}
				}
			}
			break;
	}
}

void Equip24Window::ChangeDate()
{
	struct date Solar,Lunar;
	char buffer[16];
	getdate(&Solar);
	sprintf(buffer,"%4d.%2d.%2d",Solar.da_year,Solar.da_mon,Solar.da_day);
	JKControl *ctrl=GetControl(ID_STATIC+1);
	if(ctrl){
		ctrl->SetText(buffer);
		ctrl->RedrawClient();
	}
	SolLuCvt da;
	da.SolToLuna(Solar,Lunar);
	sprintf(buffer,"(·q) %4d.%2d.%2d",Lunar.da_year,Lunar.da_mon,Lunar.da_day);
	ctrl=GetControl(ID_STATIC+2);
	if(ctrl){
		ctrl->SetText(buffer);
		ctrl->RedrawClient();
	}
}

void Equip24Window::ChangeTitleStat()
{
	char buf[35];

	JKControl *ctrl=GetControl(ID_STATIC+3);
	if(ctrl){
		sprintf(buf,"¦” : %6s    ¢‰¸a : %4s",BudaeDiscern,ObjectDiscern);
		ctrl->SetText(buf);
		ctrl->RedrawWindow();
	}
}

void Equip24Window::SellectInput(RPTR modify)
{
	Eqp24InputDialog *Input;
	Eqp24InputPublicDialog *pInput;

	if(!strcmp(ObjectDiscern,"ˆ·¥")) {
		Input=new Eqp24InputDialog(Eqp24DMan,BudaeDiscern,modify);
		if(Input){
			Input->Run();
			delete Input;
			SendMessage(SAVE_FINISH);
		}
	}
	else if(!strcmp(ObjectDiscern,"·©¤e")) {
		pInput=new Eqp24InputPublicDialog(Eqp24DMan,BudaeDiscern,modify);
		if(pInput){
			pInput->Run();
			delete pInput;
			SendMessage(SAVE_FINISH);
		}
	}
}

void Equip24Window::EvShowWindow()
{
	JKDialog::EvShowWindow();
	JKControl *ctrl=GetControl(ID_BUTTON+1);
	if(ctrl) ctrl->SetFocus();
}

BOOL Equip24Window::InitConfig()
{
	FILE *fp=fopen("equip24.cfg","rb");
	if(fp){
		fread(BudaeDiscern,7*sizeof(char),1,fp);
		BudaeDiscern[6]=0;
		fread(ObjectDiscern,5*sizeof(char),1,fp);
		ObjectDiscern[4]=0;
		fclose(fp);
		return TRUE;
	}
	return FALSE;
}

void Equip24Window::WriteConfig()
{
	FILE *fp=fopen("equip24.cfg","wb");
	if(fp){
		fwrite(BudaeDiscern,7*sizeof(char),1,fp);
		fwrite(ObjectDiscern,5*sizeof(char),1,fp);
		fclose(fp);
	}
}

void Equip24Window::EvDeadKeyDown(uint16 key,WORD /*modifier*/)
{
	WORD ctrlid;
	JKControl* ctrl;
	WORD LastCtrlID = ID_BUTTON+16;
	if(key==PGDNKEY){
		if(FindSubWindow(MsgFilter->FocusWinID,ctrlid,TRUE)&&ctrlid){
			if(ctrlid>=ID_BUTTON+1&&ctrlid<LastCtrlID) ctrlid++;
			else if(ctrlid==LastCtrlID) ctrlid=ID_BUTTON+1;
			else ctrlid=ID_BUTTON+1;

			ctrl=GetControl(ctrlid);
			if(ctrl) ctrl->SetFocus();
		}
	}
	else if(key==PGUPKEY){
		if(FindSubWindow(MsgFilter->FocusWinID,ctrlid,TRUE)&&ctrlid){
			if(ctrlid>ID_BUTTON+1&&ctrlid<=LastCtrlID) ctrlid--;
			else if(ctrlid==ID_BUTTON+1)	ctrlid=LastCtrlID;
			else ctrlid=LastCtrlID;

			ctrl=GetControl(ctrlid);
			if(ctrl) ctrl->SetFocus();
		}
	}
}

void Equip24Window::ListBoxHide()
{
	WORD ctrlid;
	JKControl *ctrl;
	WORD ctrlbox[3]={ID_LISTBOX+3,ID_LISTBOX+4,ID_LISTBOX+5};
	for(int16 i=0;i<3;i++) {
		ctrl=GetControl(ctrlbox[i]);
		if(ctrl) {
			if(ctrl->IsShow) ctrl->HideWindow();
		}
	}
	C_Value=FALSE;
}

void Equip24Window::FindName(char *title)
{

	NAME24 EqpName24;
	EquipNameListBox *list;
	EquipKindListBox *klist;

	WORD RTV;
	WORD CompKind=0x0003;

	strcpy(EqpName24.Division,"ˆ·¥");
	strcpy(EqpName24.Attached,BudaeDiscern);

	Equip24NameFind *namefind=new Equip24NameFind(EqpName24,CompKind,title);
	if(namefind) {
		RTV=namefind->Run();
		delete namefind;
	}
	if(RTV==ID_OK){
		list=(EquipNameListBox *)GetControl(ID_LISTBOX+1);
		if(list) {
			list->DestroyAll();
			list->FindListBox(EqpName24,CompKind);
			list->RedrawWindow();
		}
	}
	klist=(EquipKindListBox *)GetControl(ID_LISTBOX+2);
	if(klist){
		klist->DestroyAll();
		klist->RedrawWindow();
	}
}

void Equip24Window::FindCClass()
{

	NAME24 EqpName24;
	EquipNameListBox *list;
	EquipKindListBox *klist;

	RPTR index=1,realcount=0,tempindex;

	list=(EquipNameListBox *)GetControl(ID_LISTBOX+1);
	if(list) list->DestroyAll();
	while(index!=-1){
		tempindex=Eqp24DMan.Eqp24Kind.Search("",index,0x0040);
		if(tempindex!=-1){
			Eqp24DMan.Eqp24Kind.GetRecord(tempindex);
			Eqp24DMan.Eqp24Name.GetRecord(Eqp24DMan.Eqp24Kind.KindRecord.NameIndex);
			if(list) list->MakeString2(Eqp24DMan.Eqp24Name.NameRecord,Eqp24DMan.Eqp24Kind.KindRecord.NameIndex,realcount);
			realcount++;
			tempindex++;
		}
		index=tempindex;
	}
	if(list) {
		list->Add(realcount,new HuboBase(" *****  µa‹¡ˆa {·³“¡”a  *****",-1));
		list->RedrawWindow();
	}
	klist=(EquipKindListBox *)GetControl(ID_LISTBOX+2);
	if(klist){
		klist->DestroyAll();
		klist->RedrawWindow();
	}
}

void Equip24Window::Print()
{
	int16 count=0;
	HuboBase *hubo;
	Equip24PrintManager prt;

	HeadForm(1);

	EquipKindListBox *klist=(EquipKindListBox *)GetControl(ID_LISTBOX+2);
	if(klist){
		while(count<klist->HuboManager::NodeCount-1){
			if(count&&count%50==0){
				prt.PPutnch('=',90);
				prt.FormFeed();
				HeadForm(count/50+1);
			}
			hubo=klist->HuboManager::GetNode(count);
			prt.PPuts(hubo->TextBuff);
			prt.LineFeed();
			count++;
		}
	}
	prt.PPutnch('=',90);
	prt.LineFeed();
	prt.FormFeed();
}

void Equip24Window::HeadForm(int16 capt)
{
	char buf[70];
	date Date;
	HuboBase *hubo;

	Equip24PrintManager prt;
	getdate(&Date);
	sprintf(buf,"Â‰b·© : %4d.%2d.%2d               PAGE : -%d-",Date.da_year,Date.da_mon,Date.da_day,capt);

	prt.PPuts("                                        < 2.4¹·  ÑeÑ×  >");
	prt.LineFeed();
	prt.PPutnch(' ',45);
	prt.PPuts(buf);
	prt.LineFeed();
//	prt.LineFeed();

	EquipNameListBox *list=(EquipNameListBox *)GetControl(ID_LISTBOX+1);
	if(list&&list->CurrentIndex!=-1){
		hubo=list->GetHuboBase(list->CurrentIndex);
		if(hubo){
			prt.PPuts(hubo->TextBuff);
			prt.LineFeed();
		}
	}
	prt.PPutnch('=',90);
	prt.LineFeed();

	JKControl *ctrl=GetControl(ID_STATIC+99);
	if(ctrl){
		prt.PPuts(ctrl->GetText());
		prt.LineFeed();
	}
	prt.PPutnch('=',90);
	prt.LineFeed();

}

void Equip24Window::EvCtrlOnlyMove(WORD ctrlid)
{
	JKControl *ctrl;

	if(ctrlid>=ID_BUTTON+1&&ctrlid<=ID_BUTTON+16){
		ctrl=GetControl(ID_STATIC+4);
		if(ctrl&&ShowNumber!=(ctrlid-(ID_BUTTON+1))) {
			ctrl->SetText(HelpMessage[ctrlid-(ID_BUTTON+1)]);
			ctrl->RedrawWindow();
			ShowNumber=(ctrlid-(ID_BUTTON+1));
		}
	}
}
