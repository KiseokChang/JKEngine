#include "equipwin.hlp"
#include "equipwin.h"
#include "jkstatic.h"
#include "fltbttn.h"
#include "eqpkind.h"
#include "eqpplace.h"
#include "eqpdod.h"
#include "eqptable.h"
#include "eqpinga.h"
#include "eqpinput.h"
#include "eqpstat.h"
#include "eqplist.h"
#include "eqplstbx.h"
#include "eqpbargr.h"
#include "eqpsomo.h"
#include "joclock.h"
#include "sollucvt.h"

EquipWindow::EquipWindow(WORD ctrlid):
	JKDialog(JKRect(0,0,getmaxx(),getmaxy()),0,ctrlid),
	IsShowNum(0),CurrentBomb(10),IngaInwon(10),
	KindNumber(-1),NameNumber(-1),DefaultSetFlag(FALSE),InitYear(FALSE)
{
	WinForm=0x03;
	Depth=1;
	BorderSize=2;
	BackColor=LtGray;
	WinAttr|= WA_FIRSTSHOW | WA_CHANGEPLACE;
	SetText("Èe´¢ ‰ÅŸ¡");
}

EquipWindow::~EquipWindow()
{

}

void EquipWindow::InitWindow()
{
	JKDialog::InitWindow();
	InitOrgBomb();
	InitTable();
	ChangeDate();
}

void EquipWindow::SetupWindow()
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));

	JKButton *btn=new JKButton(JKRect(1,1,1020,25),0);
	btn->BackColor=Blue;
	btn->SetEnable(FALSE);
	btn->Depth=0;
	AddControl(btn);

	JKStatic *stc=new JOClock(JKRect(930,1,1000,25),0);
	stc->BackColor=Blue;
	stc->TextColor=Yellow;
	stc->AdjustFlag=ADJ_XYCENTER;
	AddControl(stc);

	stc=new JKStatic(JKRect(650,1,750,25),ID_STATIC+3);
	stc->BackColor=Blue;
	stc->TextColor=White;
	stc->AdjustFlag=ADJ_XYCENTER;
	AddControl(stc);

	stc=new JKStatic(JKRect(780,1,900,25),ID_STATIC+4);
	stc->BackColor=Blue;
	stc->TextColor=White;
	stc->AdjustFlag=ADJ_XYCENTER;
	AddControl(stc);

	stc=new JKStatic(JKRect(20,3,130,23),ID_STATIC+1);
	stc->SetText(TITLENAME);
	stc->BackColor=Blue;
	stc->TextColor=White;
	stc->AdjustFlag=ADJ_XYCENTER;
	AddControl(stc);

	rect.left+=1;
	rect.right-=1;
	rect.top=rect.bottom-22;
	rect.bottom=rect.top+21;
	btn=new JKButton(rect,0);
	btn->BackColor=LtBlue;
	btn->SetEnable(FALSE);
	btn->Depth=0;
	AddControl(btn);

	rect.left+=5;
	rect.right-=1;
	rect.top+=1;
	rect.bottom-=1;
	stc=new JKStatic(rect,ID_STATIC+2);
  stc->SetText("•¡  ¶‘   i ==> ¬a¶wº— ·¢…¬aÐw·¡ ¬—‹¡¡e ”¶á ‹±Ñw­AŸi Àx·a­A¶a. (0363)442-6602");
	stc->BackColor=LtBlue;
	stc->TextColor=White;
	stc->AdjustFlag=ADJ_YCENTER|ADJ_LEFT;
	AddControl(stc);

	rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.left+=3;
	rect.right=rect.left+100;
	rect.top+=29;
	rect.bottom=rect.top+35;

	btn=new JKButton(rect,ID_BUTTON+1);
	btn->SetText("Ðw  ¡¢");
	btn->BackColor=LtGray;
	btn->TextColor=Black;
	AddControl(btn);

	rect.left=rect.right+1;
	rect.right=rect.left+100;
	btn=new JKButton(rect,ID_BUTTON+2);
	btn->SetText("D.O.D");
	btn->BackColor=LtGray;
	btn->TextColor=Black;
	AddControl(btn);

	rect.left=rect.right+1;
	rect.right=rect.left+100;
	btn=new JKButton(rect,ID_BUTTON+3);
	btn->SetText("LOT-¤åÑ¡");
	btn->BackColor=LtGray;
	btn->TextColor=Black;
	AddControl(btn);

	rect.left=rect.right+1;
	rect.right=rect.left+90;
	btn=new JKButton(rect,ID_BUTTON+4);
	btn->SetText("¹A¸b‘e•¡");
	btn->BackColor=LtGray;
	btn->TextColor=Black;
	AddControl(btn);

	rect.left=rect.right+1;
	rect.right=rect.left+100;
	btn=new JKButton(rect,ID_BUTTON+5);
	btn->SetText("¥¡‰Å¸w­¡");
	btn->BackColor=LtGray;
	btn->TextColor=Black;
	AddControl(btn);

	rect.left=rect.right+1;
	rect.right=rect.left+100;
	btn=new JKButton(rect,ID_BUTTON+6);
	btn->SetText("¬a¶w¶w•¡");
	btn->BackColor=LtGray;
	btn->TextColor=Black;
	AddControl(btn);

	rect.left=rect.right+1;
	rect.right=rect.left+100;
	btn=new JKButton(rect,ID_BUTTON+7);
	btn->SetText("Á¡‹¡ÑÁ");
	btn->BackColor=LtGray;
	btn->TextColor=Black;
	AddControl(btn);

	rect.right=1010;
	rect.left=rect.right-110;
	btn=new JKButton(rect,ID_BUTTON+8);
	btn->SetText("·¥ˆa ·³b");
	btn->BackColor=LtMagenta;
	btn->TextColor=Black;
	AddControl(btn);

	rect=ClientRect.SetLeftTopBy(JKPoint(0,0));

	rect.top+=200;
	rect.bottom=rect.top+40;
	rect.right-=10;
	rect.left=rect.right-150;

	JKFloatingButton *fbtn=new JKFloatingButton(rect,ID_BUTTON+9);
	fbtn->SetText("­¡  ¡¡");
	fbtn->Depth=5;
	AddControl(fbtn);

	rect.top=rect.bottom+10;
	rect.bottom=rect.top+40;
	fbtn=new JKFloatingButton(rect,ID_BUTTON+10);
	fbtn->SetText("®  w");
	fbtn->Depth=5;
	AddControl(fbtn);

	rect.top=rect.bottom+10;
	rect.bottom=rect.top+40;
	fbtn=new JKFloatingButton(rect,ID_BUTTON+11);
	fbtn->SetText("¥¡  ‹¡");
	fbtn->Depth=5;
	AddControl(fbtn);

	rect.top=rect.bottom+10;
	rect.bottom=rect.top+40;
	fbtn=new JKFloatingButton(rect,ID_BUTTON+12);
	fbtn->SetText("¸åÁA¥¡‹¡");
	fbtn->Depth=5;
	AddControl(fbtn);

	rect.top=rect.bottom+10;
	rect.bottom=rect.top+40;
	fbtn=new JKFloatingButton(rect,ID_BUTTON+13);
	fbtn->SetText("¬wÈ¥¡‹¡");
	fbtn->Depth=5;
	AddControl(fbtn);

	rect.top=rect.bottom+10;
	rect.bottom=rect.top+40;
	fbtn=new JKFloatingButton(rect,ID_BUTTON+14);
	fbtn->SetText("¹·  ža");
	fbtn->Depth=5;
	AddControl(fbtn);

	EqpTable *table=new EqpTable(EQPUIPCONTROL,&CurrentBomb,&IngaInwon);
	AddControl(table);

	JKListBox *listbox=new JKListBox(JKRect(3,70,103,230),ID_LISTBOX+1);
	listbox->WinAttr&=~BYTE(WA_FIRSTSHOW);
	listbox->WinAttr|=WA_CHANGEPLACE;

	AddControl(listbox);

	listbox->Add(0,new HuboBase("HE",0));
	listbox->Add(1,new HuboBase("WP",1));
	listbox->Add(2,new HuboBase("ICM",2));
	listbox->Add(3,new HuboBase("DP-ICM",3));
	listbox->Add(4,new HuboBase("ILL",4));
	listbox->Add(5,new HuboBase("HEAT",5));
	listbox->Add(6,new HuboBase("RAP",6));
	listbox->Add(7,new HuboBase("RA-FAS",7));
	listbox->Add(8,new HuboBase("5.56MM",8));
	listbox->Add(9,new HuboBase("¸w´¢",9));
	listbox->Add(10,new HuboBase("®…¤i¯¥‰Å",10));
	listbox->Add(11,new HuboBase("¯¡Ðe¯¥‰Å",11));
	listbox->Add(12,new HuboBase("¸ó‹e¯¥‰Å",12));
	listbox->Add(13,new HuboBase("‰ÅÉ·¯¥‰Å",13));
	listbox->Add(14,new HuboBase("’A‰Å",14));

	EqpBarGr *grp=new EqpBarGr(ID_USERCONTROL+14,&CurrentBomb,0);
	AddControl(grp);

	EquipListWindow *listwin=new EquipListWindow(JKRect(1,27,1015,760),ID_LISTBOX+3);
	listwin->BackColor=LtGray;
	AddControl(listwin);

	EqpInput *eqpinput=new EqpInput(EQPUIPCONTROL+1);
	AddControl(eqpinput);

	rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.left+=71;
	rect.right=rect.left+170;
	rect.top=65;
	rect.bottom=rect.top+130;
	EqpDOD *dod=new EqpDOD(rect,ID_USERCONTROL,10);
	dod->SetText("D.O.D ·³b Àw");
	dod->SetExplanText("D.O.D :");
	AddControl(dod);

	rect.left=rect.right-97;
	rect.right=rect.left+170;
	dod=new EqpDOD(rect,ID_USERCONTROL+1,21);
	dod->SetText("LOT-NO ·³b Àw");
	dod->SetExplanText("LOT-NO :");
	AddControl(dod);

	rect.left=rect.right-10;
	rect.right=rect.left+100;
	EquipListBox *eqplistbox= new EquipListBox(rect,ID_LISTBOX+6);
	eqplistbox->WinAttr&=~BYTE(WA_FIRSTSHOW);
	listbox->WinAttr|=WA_CHANGEPLACE;
	AddControl(eqplistbox);

	rect.left=rect.right;
	rect.right=rect.left+100;
	rect.bottom-=20;

	listbox=new JKListBox(rect,ID_LISTBOX+5);
	listbox->WinAttr&=~BYTE(WA_FIRSTSHOW);
	listbox->WinAttr|=WA_CHANGEPLACE;
	AddControl(listbox);

	char buf[16];
	int16 i=0;
	EqpPlace *place=new EqpPlace();
	listbox->Add(0,new HuboBase("¸åÁA",i));
	while(place->Read(*buf,i)) {
		listbox->Add(i+1,new HuboBase(buf,i+1));
		i++;
	}
	rect.left=rect.right;
	rect.right=rect.left+100;
	rect.bottom-=30;
	JKListBox *list=new JKListBox(rect,ID_LISTBOX+4);
	list->WinAttr&=~BYTE(WA_FIRSTSHOW);
	list->WinAttr|=WA_CHANGEPLACE;
	AddControl(list);

	list->Add(0,new HuboBase("¸åÁA",0));
	list->Add(1,new HuboBase("B/L",1));
	list->Add(2,new HuboBase("CSR",2));
	list->Add(3,new HuboBase("ŠaÈe",3));

	EqpStat *stat=new EqpStat(ID_USERCONTROL+6);
	AddControl(stat);

}

void EquipWindow::PaintClient(JKDC& dc,JKRect& paintrect)
{
	JKPoint p(880,90);
	dc.PutIMG("log100.img",p,JKRect(0,0,130,150).OffsetBy(p),COPY_PUT);
	//dc.PutIMG("..\\bomb1.img",JKPoint(0,0),paintrect);
	JKRect rect;
	JKControl *ctrl=GetControl(ID_BUTTON+1);
	if(ctrl) {
		rect=ctrl->WinRect.ExpandBy(2);
		rect.right=ClientRect.right-3;
		dc.SolidBar(rect,White);
		dc.Rectangle(rect,Black);
	}
}

void EquipWindow::EvButtonClick(WORD ctrlid)
{
	JKControl *win;
	JKDialog *w;
	EquipListWindow *listwin;
	JKButton *btn;
	BombBase *bombbase;

	AfterHide();
	switch(ctrlid){
		case ID_BUTTON+1:
			win=GetControl(ID_LISTBOX+1);
			if(win)win->ShowWindow();
			break;
		case ID_BUTTON+2:
			win=GetControl(ID_USERCONTROL);
			if(win) win->ShowWindow();
			break;
		case ID_BUTTON+3:
			win=GetControl(ID_USERCONTROL+1);
			if(win) win->ShowWindow();
			break;
		case ID_BUTTON+4:
			win=GetControl(ID_LISTBOX+6);
			if(win) win->ShowWindow();
			break;
		case ID_BUTTON+5:
			win=GetControl(ID_LISTBOX+5);
			if(win) win->ShowWindow();
			break;
		case ID_BUTTON+6:
			win=GetControl(ID_LISTBOX+4);
			if(win) win->ShowWindow();
			break;
		case ID_BUTTON+7:
			if(DefaultSetFlag) {
				DefaultSetFlag=FALSE;
				OrgBomb=TempBomb;
				EqpTable *table=(EqpTable *)GetControl(EQPUIPCONTROL);
					if(table) btn=(JKButton *)table->GetControl(ID_BUTTON+1);
					if(btn) {
						btn->SetText("¶w•¡:¸åÁA");
						btn->RedrawWindow();
					}
				InitTable();
			}
			break;
		case ID_BUTTON+8:
			EqpInga *inga;
			if(!strcmp(OrgBomb.Use,"B/L")) {
				inga=new EqpInga(BL_NAME,10);
				if(inga) inga->Run();
				if(inga) delete inga;
				InitTable();
			}
			else if(!strcmp(OrgBomb.Use,"CSR")) {
				inga=new EqpInga(CSR_NAME,10);
				if(inga) inga->Run();
				if(inga) delete inga;
				InitTable();
			}
			else if(!strcmp(OrgBomb.Use,"ŠaÈe")) {
				inga=new EqpInga(EDU_NAME,10);
				if(inga) inga->Run();
				if(inga) delete inga;
				InitTable();
			}
			break;
		case ID_BUTTON+9:
			{
			JKRect rect=GetClientRectOnScreen();
			rect.top+=27;
			EquipSomoWindow *somo=new EquipSomoWindow(rect);
			if(somo) {
				somo->ListBomb=OrgBomb;
				if(NameNumber==4){
					if(KindNumber!=-1&&strcmp(somo->ListBomb.Kind,Kind[KindNumber])&&KindNumber!=15&&*somo->ListBomb.Kind!=0) break;
				}
				if(KindNumber==15) {
					if(NameNumber!=-1)strcpy(somo->ListBomb.Name,Name[NameNumber]);
				}
				else {
					bombbase=CurrentBomb.GetNode(KindNumber);
					if(bombbase){
						if(KindNumber!=-1) strcpy(somo->ListBomb.Kind,Kind[KindNumber]);
						if(NameNumber!=-1) strcpy(somo->ListBomb.Name,Name[NameNumber]);
					}
				}
				somo->Run();
				InitTable();
				delete somo;
			}
			}
			break;
		case ID_BUTTON+10:
			SendMessage(ID_RECINPUT);
			break;
		case ID_BUTTON+11:

			listwin=(EquipListWindow *) GetControl(ID_LISTBOX+3);

			if(listwin) {
				listwin->ListBomb=OrgBomb;
				if(NameNumber==4){
					if(KindNumber!=-1&&strcmp(listwin->ListBomb.Kind,Kind[KindNumber])&&KindNumber!=15&&*listwin->ListBomb.Kind!=0) break;
				}
				if(KindNumber==15) {
					if(NameNumber!=-1)strcpy(listwin->ListBomb.Name,Name[NameNumber]);
				}
				else {
					bombbase=CurrentBomb.GetNode(KindNumber);
					if(bombbase){
						if(KindNumber!=-1) strcpy(listwin->ListBomb.Kind,bombbase->Text);
						if(NameNumber!=-1) strcpy(listwin->ListBomb.Name,Name[NameNumber]);
					}
				}
				listwin->CreatListEntry();
				listwin->ShowWindow();
			}
			break;
		case ID_BUTTON+12:
			listwin=(EquipListWindow *) GetControl(ID_LISTBOX+3);
			if(listwin){
				listwin->InitListBomb();
				listwin->CreatListEntry();
				listwin->ShowWindow();
			}
			break;
		case ID_BUTTON+13:
			{
			EqpStat *stat=(EqpStat *)GetControl(ID_USERCONTROL+6);
			if(stat) {
				stat->SetBomb(OrgBomb);
				stat->ShowWindow();
			}
			}
			break;
		case ID_BUTTON+14:
			CloseWindow();
			break;
	}
}

void EquipWindow::EvCharDown(uint16 key,WORD /*ctrlid*/)
{
	if(key==ESC) SendMessage(CLOSEWINDOW);
}

ProcessResult EquipWindow::RespondMessage(JKMSG& msg)
{
	EqpInput *w;

	switch(msg.main) {
		case ID_RECINPUT:
			w=(EqpInput *) GetControl(EQPUIPCONTROL+1);
			if(w) {
				w->ShowWindow();
			}

			break;
		case ID_RECMODIFY:
			w=(EqpInput *) GetControl(EQPUIPCONTROL+1);
			if(w) {
				w->MODIFY=TRUE;
				w->SetValue(HIWORD(msg.sub));
				w->ShowWindow();
			}
			break;
		case ID_SAVEFINISH:
			{
			EquipListWindow *wi=(EquipListWindow *)GetControl(ID_LISTBOX+3);
			wi->ReForm();
			}
			break;
		case ID_SAVE:
			SaveBomb();
			//InitOrgBomb();
			InitTable();
			break;
		case INWON_SELECTCELL:
			if((int16)HIWORD(msg.detail)!=-1) NameNumber=HIWORD(msg.detail)-1;
			else NameNumber=4;
			if((int16)LOWORD(msg.detail)!=-1) KindNumber=LOWORD(msg.detail);
			else KindNumber=15;
			break;
		case EQPDODMESSAGE:
    	{
			if(!DefaultSetFlag) {
				DefaultSetFlag=TRUE;
				TempBomb=OrgBomb;
			}
			EqpDOD *dod=(EqpDOD *) GetControl(ID_USERCONTROL);
			if(dod) strcpy(OrgBomb.Dod,dod->Buffer);
			dod=(EqpDOD *) GetControl(ID_USERCONTROL+1);
			if(dod) strcpy(OrgBomb.Lot,dod->Buffer);
			InitTable();
			}
			break;
		case ID_REFLUSH:
			InitTable();
			break;
		case CLOCK_CHANGEDATE:
			ChangeDate();
			break;
		case INWON_SETFIRSTINDEX:
			EqpBarGr *grp=(EqpBarGr *)GetControl(ID_USERCONTROL+14);
			if(grp) {
				int16 tt=HIWORD(msg.sub);
				grp->FirstShowIndex=tt;
				grp->SetValue();
				grp->RedrawWindow();
			}
			break;
	}
	return JKDialog::RespondMessage(msg);
}

void EquipWindow::EvLBSelectEntry(WORD ctrlid,uint16 index)
{
	HuboBase *hubo;
	JKListBox *pop;
	JKButton *btn;
	char buf[10];

	switch(ctrlid) {
		case ID_LISTBOX+1:
			pop=(JKListBox *) GetControl(ID_LISTBOX+1);
			if(pop) {
				hubo=pop->GetHuboBase(index);
				if(!DefaultSetFlag) {
					DefaultSetFlag=TRUE;
					TempBomb=OrgBomb;
				}
				if(hubo) strcpy(OrgBomb.Kind,hubo->TextBuff);
				pop->HideWindow();
				InitTable();
			}
			break;

		case ID_LISTBOX+4:
			pop=(JKListBox *) GetControl(ID_LISTBOX+4);
			if(pop) {
				hubo=pop->GetHuboBase(index);
				if(!DefaultSetFlag) {
					DefaultSetFlag=TRUE;
					TempBomb=OrgBomb;
				}
				if(hubo) {
					if(index==0) strcpy(OrgBomb.Use,"");
					else strcpy(OrgBomb.Use,hubo->TextBuff);
					EqpTable *table=(EqpTable *)GetControl(EQPUIPCONTROL);
					if(table) btn=(JKButton *)table->GetControl(ID_BUTTON+1);
					if(btn) {
						sprintf(buf,"¶w•¡:%s",hubo->TextBuff);
						btn->SetText(buf);
						btn->RedrawWindow();
					}
				}
				pop->HideWindow();
				InitTable();
			}
			break;
		case ID_LISTBOX+5:
			pop=(JKListBox *) GetControl(ID_LISTBOX+5);
			if(pop) {
				hubo=pop->GetHuboBase(index);
				if(!DefaultSetFlag) {
					DefaultSetFlag=TRUE;
					TempBomb=OrgBomb;
				}
				if(hubo) {
					if(index==0) strcpy(OrgBomb.Place,"");
					else strcpy(OrgBomb.Place,hubo->TextBuff);
				}
				pop->HideWindow();
				InitTable();
			}
			break;
    case ID_LISTBOX+6:
			pop=(EquipListBox *) GetControl(ID_LISTBOX+6);
			if(pop) {
				hubo=pop->GetHuboBase(index);
				if(!DefaultSetFlag) {
					DefaultSetFlag=TRUE;
					TempBomb=OrgBomb;
				}
				if(hubo) {
					if(index==0) strcpy(OrgBomb.Born,"");
					else strcpy(OrgBomb.Born,hubo->TextBuff);
				}
				pop->HideWindow();
				InitTable();
			}
			break;
	}
}

void EquipWindow::EvLBCharDown(WORD ctrlid,uint16 index,uint16 key,WORD /*modifier*/)
{
	HuboBase *hubo;
	JKListBox *pop;
	JKButton *btn;
	char buf[10];

  if(key==ENTER) {
	  switch(ctrlid) {
		  case ID_LISTBOX+1:
				pop=(JKListBox *) GetControl(ID_LISTBOX+1);
			  if(pop) {
				  hubo=pop->GetHuboBase(index);
				  if(!DefaultSetFlag) {
					  DefaultSetFlag=TRUE;
					  TempBomb=OrgBomb;
				  }
				  if(hubo) strcpy(OrgBomb.Kind,hubo->TextBuff);
				  pop->HideWindow();
				  InitTable();
			  }
				break;

		  case ID_LISTBOX+4:
			  pop=(JKListBox *) GetControl(ID_LISTBOX+4);
			  if(pop) {
				  hubo=pop->GetHuboBase(index);
					if(!DefaultSetFlag) {
					  DefaultSetFlag=TRUE;
					  TempBomb=OrgBomb;
				  }
				  if(hubo) {
					  if(index==0) strcpy(OrgBomb.Use,"");
					  else strcpy(OrgBomb.Use,hubo->TextBuff);
					  EqpTable *table=(EqpTable *)GetControl(EQPUIPCONTROL);
					  if(table) btn=(JKButton *)table->GetControl(ID_BUTTON+1);
					  if(btn) {
						  sprintf(buf,"¶w•¡:%s",hubo->TextBuff);
						  btn->SetText(buf);
						  btn->RedrawWindow();
					  }
				  }
				  pop->HideWindow();
				  InitTable();
			  }
				break;
		  case ID_LISTBOX+5:
			  pop=(JKListBox *) GetControl(ID_LISTBOX+5);
			  if(pop) {
				  hubo=pop->GetHuboBase(index);
				  if(!DefaultSetFlag) {
					  DefaultSetFlag=TRUE;
					  TempBomb=OrgBomb;
				  }
				  if(hubo) {
					  if(index==0) strcpy(OrgBomb.Place,"");
						else strcpy(OrgBomb.Place,hubo->TextBuff);
				  }
				  pop->HideWindow();
				  InitTable();
			  }
			  break;
			case ID_LISTBOX+6:
			  pop=(EquipListBox *) GetControl(ID_LISTBOX+6);
			  if(pop) {
				  hubo=pop->GetHuboBase(index);
				  if(!DefaultSetFlag) {
					  DefaultSetFlag=TRUE;
					  TempBomb=OrgBomb;
				  }
				  if(hubo) {
					  if(index==0) strcpy(OrgBomb.Born,"");
					  else strcpy(OrgBomb.Born,hubo->TextBuff);
				  }
				  pop->HideWindow();
				  InitTable();
			  }
			  break;
	  }
  }
}

void EquipWindow::EvCtrlOnlyMove(WORD ctrlid)
{
	JKControl *win;
	win=GetControl(ID_STATIC+2);
	if(win) {
		switch(ctrlid) {
			case ID_BUTTON+1:
				if(IsShowNum!=ID_BUTTON+1){
					IsShowNum=ID_BUTTON+1;
					win->SetText(Help[0]);
					win->RedrawWindow();
				}
				break;
			case ID_BUTTON+2:
				if(IsShowNum!=ID_BUTTON+2) {
					IsShowNum=ID_BUTTON+2;
					win->SetText(Help[1]);
					win->RedrawWindow();
				}
				break;
			case ID_BUTTON+3:
				if(IsShowNum!=ID_BUTTON+3) {
					IsShowNum=ID_BUTTON+3;
					win->SetText(Help[2]);
					win->RedrawWindow();
				}
				break;
			case ID_BUTTON+4:
				if(IsShowNum!=ID_BUTTON+4) {
					IsShowNum=ID_BUTTON+4;
					win->SetText(Help[3]);
					win->RedrawWindow();
				}
				break;
			case ID_BUTTON+5:
				if(IsShowNum!=ID_BUTTON+5) {
					IsShowNum=ID_BUTTON+5;
					win->SetText(Help[4]);
					win->RedrawWindow();
				}
				break;
			case ID_BUTTON+6:
				if(IsShowNum!=ID_BUTTON+6) {
					IsShowNum=ID_BUTTON+6;
					win->SetText(Help[5]);
					win->RedrawWindow();
				}
				break;
			case ID_BUTTON+7:
				if(IsShowNum!=ID_BUTTON+7) {
					IsShowNum=ID_BUTTON+7;
					win->SetText(Help[6]);
					win->RedrawWindow();
				}
				break;
			case ID_BUTTON+8:
				if(IsShowNum!=ID_BUTTON+8) {
					IsShowNum=ID_BUTTON+8;
					win->SetText(Help[7]);
					win->RedrawWindow();
				}
				break;
			case ID_BUTTON+9:
				if(IsShowNum!=ID_BUTTON+9) {
					IsShowNum=ID_BUTTON+9;
					win->SetText(Help[8]);
					win->RedrawWindow();
				}
				break;
			case ID_BUTTON+10:
				if(IsShowNum!=ID_BUTTON+10) {
					IsShowNum=ID_BUTTON+10;
					win->SetText(Help[9]);
					win->RedrawWindow();
				}
				break;
			case ID_BUTTON+11:
				if(IsShowNum!=ID_BUTTON+11) {
					IsShowNum=ID_BUTTON+11;
					win->SetText(Help[10]);
					win->RedrawWindow();
				}
				break;
			case ID_BUTTON+12:
				if(IsShowNum!=ID_BUTTON+12) {
					IsShowNum=ID_BUTTON+12;
					win->SetText(Help[11]);
					win->RedrawWindow();
				}
				break;
			case ID_BUTTON+13:
				if(IsShowNum!=ID_BUTTON+13) {
					IsShowNum=ID_BUTTON+13;
					win->SetText(Help[12]);
					win->RedrawWindow();
				}
				break;
			case ID_BUTTON+14:
				if(IsShowNum!=ID_BUTTON+14) {
					IsShowNum=ID_BUTTON+14;
					win->SetText(Help[13]);
					win->RedrawWindow();
				}
			break;
		}
	}
}

void EquipWindow::EvOnlyMove(JKPoint&)
{
	JKControl *win;
	win=GetControl(ID_STATIC+2);
	if(win) {
		win->SetText("•¡  ¶‘   i ==> ¬a¶wº— ·¢…¬aÐw·¡ ¬—‹¡¡e ”¶á ‹±Ñw­AŸi Àx·a­A¶a. (0363)442-6602");
		win->RedrawWindow();
		IsShowNum=0;
	}
}

void EquipWindow::AfterHide()
{
	JKControl *ctl;
	ctl=GetControl(ID_LISTBOX+1);
	if(ctl) ctl->HideWindow();
	ctl=GetControl(ID_LISTBOX+3);
	if(ctl) ctl->HideWindow();
	ctl=GetControl(ID_LISTBOX+4);
	if(ctl) ctl->HideWindow();
	ctl=GetControl(ID_LISTBOX+5);
	if(ctl) ctl->HideWindow();
	ctl=GetControl(ID_LISTBOX+6);
	if(ctl) ctl->HideWindow();
	for(int16 i=0;i<7;i++) {
		ctl=GetControl(ID_USERCONTROL+i);
		if(ctl) if(ctl->IsShow) ctl->HideWindow();
	}
}

void EquipWindow::SaveBomb()
{
	EqpInput *input=(EqpInput *)GetControl(EQPUIPCONTROL+1);
	if(input&&!input->IsEmpty()){
		EqiupFileManage *fm=new EqiupFileManage(EQUIPNAME);
		if(fm){
			input->CheckBomb();
			if(!input->MODIFY) {
				fm->Append(input->Bomb);
				input->RedrawWindow();
			}
			else {
				EquipListWindow *win=(EquipListWindow *)GetControl(ID_LISTBOX+3);
				if(win) {
					fm->Modify(input->Bomb,win->CurrentIndex);
					input->MODIFY=FALSE;
					input->HideWindow();
				}
			}
			input->ClearAllText();
			input->GetValue();
			delete fm;
			SendMessage(ID_SAVEFINISH);
		}
	}
}

void EquipWindow::InitTable()
{
	if(!strcmp(OrgBomb.Use,"CSR")) IngaInwon.GetInwon(CSR_NAME);
	else if(!strcmp(OrgBomb.Use,"B/L")) IngaInwon.GetInwon(BL_NAME);
	else if(!strcmp(OrgBomb.Use,"ŠaÈe")) IngaInwon.GetInwon(EDU_NAME);
	else IngaInwon.ClearNode();

	CurrentBomb.ClearNode();
	EqpTable *table =(EqpTable *)GetControl(EQPUIPCONTROL);
	InitBombData();

	EqpBarGr *grp=(EqpBarGr *)GetControl(ID_USERCONTROL+14);
	if(grp) {
		grp->SetValue();
		grp->RedrawWindow();
	}

	if(table)	table->RedrawTable();
}

void EquipWindow::InitBombData()
{

	BOMB Bomb;
	int16 k=0;

	EqiupFileManage *fm=new EqiupFileManage(EQUIPNAME);
	if(fm) {
		MouseEvents->SetCursorID(5);
		while(fm->Read(Bomb,k++)){
			if(!InitYear)InitEquipListBox(Bomb);
			for(int16 i=0;i<4;i++){
				if(strcmp(Bomb.Name,Name[i])==0)
					if(FindBomb(Bomb)) CurrentBomb.Add(i,Bomb.Kind,atoi(Bomb.Count));
			}
		}
		delete fm;
		if(!InitYear) {
			EquipListBox *listbox=(EquipListBox *) GetControl(ID_LISTBOX+6);
			if(listbox) listbox->HuboManager::PutNode(new HuboBase("¸åÁA"),0);
			InitYear=TRUE;
		}
	}
}

BOOL EquipWindow::FindBomb(BOMB& bm)
{
	if(strcmp(OrgBomb.Kind,bm.Kind)==0||*OrgBomb.Kind==0) {
		if(strcmp(OrgBomb.Name,bm.Name)==0||*OrgBomb.Name==0) {
			if(strcmp(OrgBomb.Dod,bm.Dod)==0||*OrgBomb.Dod==0) {
				if(strcmp(OrgBomb.Lot,bm.Lot)==0||*OrgBomb.Lot==0) {
					if(strstr(bm.Born,OrgBomb.Born)!=0||*OrgBomb.Born==0) {
						if(strcmp(OrgBomb.Place,bm.Place)==0||*OrgBomb.Place==0) {
							if(strcmp(OrgBomb.Use,bm.Use)==0||*OrgBomb.Use==0) {
								return TRUE;
							}
						}
					}
				}
			}
		}
	}
	return FALSE;
}

void EquipWindow::InitOrgBomb()
{
	*OrgBomb.Name=0;
	*OrgBomb.Kind=0;
	*OrgBomb.Dod=0;
	*OrgBomb.Lot=0;
	*OrgBomb.Born=0;
	*OrgBomb.Place=0;
	*OrgBomb.Use=0;
}

void EquipWindow::InitEquipListBox(BOMB& bm)
{
	char Year[5];

	EquipListBox *listbox=(EquipListBox *) GetControl(ID_LISTBOX+6);
	if(listbox) {
		strncpy(Year,bm.Born,4);
		Year[4]=NULL;
		listbox->Add(Year);
	}
}

void EquipWindow::ChangeDate()
{
	struct date Solar,Lunar;
	char buffer[16];
	getdate(&Solar);
	sprintf(buffer,"%4d.%2d.%2d",Solar.da_year,Solar.da_mon,Solar.da_day);
	JKControl *ctrl=GetControl(ID_STATIC+3);
	if(ctrl){
		ctrl->SetText(buffer);
		ctrl->RedrawClient();
	}
	SolLuCvt da;
	da.SolToLuna(Solar,Lunar);
	sprintf(buffer,"(·q) %4d.%2d.%2d",Lunar.da_year,Lunar.da_mon,Lunar.da_day);
	ctrl=GetControl(ID_STATIC+4);
	if(ctrl){
		ctrl->SetText(buffer);
		ctrl->RedrawClient();
	}
}

void EquipWindow::EvShowWindow()
{
	JKDialog::EvShowWindow();
	JKControl *win=GetControl(ID_BUTTON+1);
	if(win) win->SetFocus();
}

void EquipWindow::EvDeadKeyDown(uint16 key,WORD /*modifier*/)
{
	WORD CtrlID[14]={ID_BUTTON+1,ID_BUTTON+2,ID_BUTTON+3,ID_BUTTON+4,ID_BUTTON+5,
				ID_BUTTON+6,ID_BUTTON+7,ID_BUTTON+8,ID_BUTTON+9,ID_BUTTON+10,ID_BUTTON+11,
				ID_BUTTON+12,ID_BUTTON+13,ID_BUTTON+14};
	WORD ctrlid;

	if(key==PGDNKEY){
		if(FindSubWindow(MsgFilter->FocusWinID,ctrlid,TRUE)&&ctrlid){
			JKControl* ctrl;
			if(ctrlid>=CtrlID[0]&&ctrlid<CtrlID[13]) ctrlid++;
			else if(ctrlid==CtrlID[13]) ctrlid=CtrlID[0];
			else if(ctrlid==CtrlID[0]) ctrlid=CtrlID[13];
			else ctrlid=CtrlID[0];

			ctrl=GetControl(ctrlid);
			if(ctrl) ctrl->SetFocus();
		}
	}
	else if(key==PGUPKEY){
		if(FindSubWindow(MsgFilter->FocusWinID,ctrlid,TRUE)&&ctrlid){
			JKControl* ctrl;

			if(ctrlid>CtrlID[0]&&ctrlid<=CtrlID[13]) ctrlid--;
			else if(ctrlid==CtrlID[0])	ctrlid=CtrlID[13];
			else if(ctrlid==CtrlID[13]) ctrlid=CtrlID[0];
			else ctrlid=CtrlID[0];

			ctrl=GetControl(ctrlid);
			if(ctrl) ctrl->SetFocus();
		}
	}
	if(key==F10){
		JKControl* ctrl=GetControl(CtrlID[0]);
		if(ctrl)ctrl->SetFocus();
	}
}