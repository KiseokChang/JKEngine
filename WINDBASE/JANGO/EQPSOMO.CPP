#include "eqpsomo.h"
#include "jkstatic.h"
#include "fltbttn.h"
#include "eqfilemn.h"
#include "eqpsmip.h"
#include "eqpprint.h"
#include "eqpview.h"
#include "extnval.h"

EquipSomoWindow::EquipSomoWindow(JKRect rect,JKControl *parent):
	JKDialog(rect,parent),CurrentIndex(-1)
{
	WinForm=0;
	InitListBomb();
}

EquipSomoWindow::~EquipSomoWindow()
{}

void EquipSomoWindow::SetupWindow()
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));

	rect.left+=20;
	rect.right-=300;
	rect.top+=22;
	rect.bottom=rect.top+25;;

	JKStatic *stc=new JKStatic(rect,ID_STATIC);
	stc->SetText(" ­¡ ­¢   ¥¡‰Å¸w­¡    Ðw ¡¢    D.O.D      ¤i®  ¬a¶w¶w•¡  ®w·©¸a ");
	stc->BackColor=Gray;
	stc->TextColor=White;
	stc->AdjustFlag=ADJ_LEFT|ADJ_YCENTER;
	AddControl(stc);

	rect.top=rect.bottom+5;
	rect.bottom=rect.top+630;
	ScrollListBox *scrlistbox=new ScrollListBox(rect,ID_LISTBOX);
	scrlistbox->BackColor=LtCyan;
	AddControl(scrlistbox);
	CreatListEntry();

  rect.top=100;rect.bottom=rect.top+50;
	rect.left=rect.right+100;rect.right=rect.left+150;
	JKFloatingButton *fbtn=new JKFloatingButton(rect,ID_BUTTON+1);
	fbtn->SetText("­¡  ¡¡");
	AddControl(fbtn);

  rect.top=rect.bottom+50;rect.bottom=rect.top+50;
	fbtn=new JKFloatingButton(rect,ID_BUTTON+2);
	fbtn->SetText("¥¡  ‹¡");
	AddControl(fbtn);

  rect.top=rect.bottom+50;rect.bottom=rect.top+50;
	fbtn=new JKFloatingButton(rect,ID_BUTTON+3);
	fbtn->SetText("­¡¡¡ÑeÑ×");
	AddControl(fbtn);

  rect.top=rect.bottom+50;rect.bottom=rect.top+50;
	fbtn=new JKFloatingButton(rect,ID_BUTTON+4);
	fbtn->SetText("·¡¸å¬wÈ");
	AddControl(fbtn);

  rect.top=rect.bottom+50;rect.bottom=rect.top+50;
	fbtn=new JKFloatingButton(rect,ID_BUTTON+5);
	fbtn->SetText("´|·a¡");
	AddControl(fbtn);

	JKControl *ctl=GetControl(ID_BUTTON+3);
	if(ctl) {
		fbtn=new JKFloatingButton(ctl->WinRect,ID_BUTTON+6);
		fbtn->SetText("ÏaŸ¥Ëa");
		fbtn->WinAttr&=~BYTE(WA_FIRSTSHOW);
		AddControl(fbtn);
	}
}

void EquipSomoWindow::PaintClient(JKDC& dc,JKRect&)
{
	JKControl *win=GetControl(ID_LISTBOX);
	JKRect rect;
	if(win) {
		rect=win->WinRect.ExpandBy(1);
		dc.Rectangle3D(rect,1,White,Black);
	}
	win=GetControl(ID_STATIC);
	if(win) {
		rect=win->WinRect.ExpandBy(1);
		dc.Rectangle3D(rect,1,White,Black);
	}
}

void EquipSomoWindow::EvButtonClick(WORD ctrlid)
{
	switch(ctrlid) {
		case ID_BUTTON+1:
		case ID_BUTTON+2:
		case ID_BUTTON+4:
    	{
			JKListBox *lst=(JKListBox *) GetControl(ID_LISTBOX);
			if(lst){
				switch(ctrlid) {
					case ID_BUTTON+1:
						if(lst->CurrentIndex!=-1&&CurrentIndex!=-1) {
							EquipSomoInputWindow *soip=new EquipSomoInputWindow(0);
							if(soip) {
								soip->Index=CurrentIndex;
								soip->InitSomo();
								soip->Run();
								delete soip;
								ReForm();
							}
						}
						break;
					case ID_BUTTON+2:
						if(lst->CurrentIndex!=-1&&CurrentIndex!=-1) SendMessage(ID_RECDVIEW2,CurrentIndex);
						break;
					case ID_BUTTON+4:
						CreatListEntry();
						SetButtonState(TRUE);
						PrintButtonChange(FALSE);
						lst->RedrawWindow();
						break;
				}
			}
			}
			break;
		case ID_BUTTON+3:
			SendMessage(ID_MODECHANGE);
			PrintButtonChange(TRUE);
			break;
		case ID_BUTTON+5:
			SendMessage(ID_OK);
			break;

		case ID_BUTTON+6:
			EquipPrint *prt=new EquipPrint();
			if(prt) {
				prt->CompareBomb=ListBomb;
				prt->SomoFlag=TRUE;
				prt->Run();
				delete prt;
			}
			break;
	}
}

void EquipSomoWindow::EvLBTouchEntry(WORD ctrlid,uint16 index)
{
	switch (ctrlid) {
		case ID_LISTBOX:
			ScrollListBox *slb=(ScrollListBox *)GetControl(ID_LISTBOX);
			if(slb){
				HuboBase *hubo=slb->GetHuboBase(index);
				if(hubo) CurrentIndex=hubo->Number;
			}
			break;
	}
}

void EquipSomoWindow::EvLBSelectEntry(WORD ctrlid,uint16 index)
{
	switch (ctrlid) {
		case ID_LISTBOX:
			ScrollListBox *slb=(ScrollListBox *)GetControl(ID_LISTBOX);
			if(slb){
				HuboBase *hubo=slb->GetHuboBase(index);
				if(hubo){
					CurrentIndex=hubo->Number;
					if(CurrentIndex!=-1) SendMessage(ID_RECDVIEW2,CurrentIndex);
				}
			}
			break;
	}
}

ProcessResult EquipSomoWindow::RespondMessage(JKMSG& msg)
{
	switch (msg.main) {
		case ID_RECDVIEW2:
      {
			EqpView *view=new EqpView();
			if(view) {
				view->SetBomb(CurrentIndex);
				view->Run();
				delete view;
			}
			}
			break;
		case ID_MODECHANGE:
			CreatSomoEntry();
			ChangeSomoTitle();
			SetButtonState(FALSE);
			RedrawWindow();
			break;
	}
	return JKDialog::RespondMessage(msg);
}

void EquipSomoWindow::RecDelete(int16 index)
{
	EqiupFileManage *fm=new EqiupFileManage(EQUIPNAME);
	if(fm) {
		fm->Delete(index);
		delete fm;
		ReForm();
	}
}

void EquipSomoWindow::ReForm()
{
	ScrollListBox *slb=(ScrollListBox *)GetControl(ID_LISTBOX);
	if(slb) {
		CreatListEntry();
		slb->RedrawClient();
	}
}

void EquipSomoWindow::CreatListEntry()
{
	ScrollListBox *scrlistbox=(ScrollListBox *) GetControl(ID_LISTBOX);
	if(scrlistbox){
		scrlistbox->DestroyAll();
		EqiupFileManage *fm=new EqiupFileManage(EQUIPNAME);
		if(fm){
			int16 i=0,j=0;
			char buf[100];
			BOMB bm;
			MouseEvents->SetCursorID(5);
			while(fm->Read(bm,i)){
				if(FindBomb(bm)) {
					sprintf(buf," %-6s  %-10s  %-8s %-10s  %-5s %-4s     %-10s",bm.Name,bm.Place,bm.Kind,bm.Dod,bm.Count,bm.Use,bm.HaveYear);
					scrlistbox->Add(j++,new HuboBase(buf,i));
				}
				i++;
			}
			scrlistbox->Add(j,new HuboBase(" *******  µa‹¡ˆa {·³“¡”a.  *********",-1));
			delete fm;
		}
	}
}

void EquipSomoWindow::CreatSomoEntry()
{
	ScrollListBox *scrlistbox=(ScrollListBox *) GetControl(ID_LISTBOX);
	if(scrlistbox){
		scrlistbox->DestroyAll();
		EqiupFileManage *fm=new EqiupFileManage(SOMONAME);
		if(fm){
			int16 i=0,j=0;
			char buf[100];
			BOMB bm;
			MouseEvents->SetCursorID(5);
			while(fm->Read(bm,i)){
				sprintf(buf," %-6s  %-10s  %-8s %-10s  %-5s %-4s     %-10s",bm.Name,bm.Place,bm.Kind,bm.Dod,bm.Count,bm.Use,bm.SomoYear);
				scrlistbox->Add(j++,new HuboBase(buf,i));
				i++;
			}
			scrlistbox->Add(j,new HuboBase(" *******  µa‹¡ˆa {·³“¡”a.  *********",-1));
			delete fm;
		}
	}
}

BOOL EquipSomoWindow::FindBomb(BOMB& bm)
{
 if(strcmp(ListBomb.Name,bm.Name)==0||*ListBomb.Name==0) {
	 if(strcmp(ListBomb.Kind,bm.Kind)==0||*ListBomb.Kind==0) {
		 if(strcmp(ListBomb.Dod,bm.Dod)==0||*ListBomb.Dod==0) {
			 if(strcmp(ListBomb.Lot,bm.Lot)==0||*ListBomb.Lot==0) {
				 if(strstr(bm.Born,ListBomb.Born)!=0||*ListBomb.Born==0) {
					 if(strcmp(ListBomb.Place,bm.Place)==0||*ListBomb.Place==0) {
						 if(strcmp(ListBomb.Use,bm.Use)==0||*ListBomb.Use==0) {
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

void EquipSomoWindow::InitListBomb()
{
	*ListBomb.Name=0;
	*ListBomb.Kind=0;
	*ListBomb.Dod=0;
	*ListBomb.Lot=0;
	*ListBomb.Born=0;
	*ListBomb.Place=0;
	*ListBomb.Use=0;
}

void EquipSomoWindow::ChangeSomoTitle()
{
	JKControl *ctl=GetControl(ID_STATIC);
	if(ctl){
		ctl->SetText(" ­¡ ­¢   ¥¡‰Å¸w­¡    Ðw ¡¢    D.O.D      ¤i®  ¬a¶w¶w•¡  ­¡¡¡·©¸a ");
		ctl->RedrawWindow();
	}
}

void EquipSomoWindow::SetButtonState(BOOL flag)
{
	JKButton *btn=(JKButton *)GetControl(ID_BUTTON+1);
	if(btn) btn->SetEnable(flag);
	btn=(JKButton *)GetControl(ID_BUTTON+3);
	if(btn) btn->SetEnable(flag);
}

void EquipSomoWindow::PrintButtonChange(BOOL flag)
{
	JKControl *ctl=GetControl(ID_BUTTON+3);
	JKControl *ctl2=GetControl(ID_BUTTON+6);
	if(ctl&&ctl2) {
		if(flag) {
			ctl->HideWindow();
			ctl2->ShowWindow();
		}
		else {
			ctl2->HideWindow();
			ctl->ShowWindow();
		}
	}
}
