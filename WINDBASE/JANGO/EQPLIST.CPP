#include "eqplist.h"
#include "jkstatic.h"
#include "fltbttn.h"
#include "eqfilemn.h"
#include "eqpview.h"
#include "eqpprint.h"
#include "extnval.h"

EquipListWindow::EquipListWindow(JKRect rect,WORD ctrlid,JKControl *parent):
	JKWindow(rect,parent,ctrlid),CurrentIndex(-1)
{
	WinForm=0;
	InitListBomb();
}

EquipListWindow::~EquipListWindow()
{}

void EquipListWindow::SetupWindow()
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

	rect.top=rect.bottom+5;	rect.bottom=rect.top+600;
	ScrollListBox *scrlistbox=new ScrollListBox(rect,ID_LISTBOX);
	scrlistbox->BackColor=LtCyan;
	AddControl(scrlistbox);

	rect.top=100;rect.bottom=rect.top+50;
	rect.left=rect.right+100;rect.right=rect.left+150;
	JKFloatingButton *fbtn=new JKFloatingButton(rect,ID_BUTTON+1);
	fbtn->SetText("»¡¶‹¡");
	AddControl(fbtn);

  rect.top=rect.bottom+50;rect.bottom=rect.top+50;
	fbtn=new JKFloatingButton(rect,ID_BUTTON+2);
	fbtn->SetText("¥¡  ‹¡");
	AddControl(fbtn);

  rect.top=rect.bottom+50;rect.bottom=rect.top+50;
	fbtn=new JKFloatingButton(rect,ID_BUTTON+3);
	fbtn->SetText("®  ¸÷");
	AddControl(fbtn);

  rect.top=rect.bottom+50;rect.bottom=rect.top+50;
	fbtn=new JKFloatingButton(rect,ID_BUTTON+4);
	fbtn->SetText("·³  b");
	AddControl(fbtn);

  rect.top=rect.bottom+50;rect.bottom=rect.top+50;
	fbtn=new JKFloatingButton(rect,ID_BUTTON+5);
  fbtn->SetText("ÏaŸ¥Ëa");
	AddControl(fbtn);

  rect.top=rect.bottom+50;rect.bottom=rect.top+50;
	fbtn=new JKFloatingButton(rect,ID_BUTTON+6);
	fbtn->SetText("´|·a¡");
	AddControl(fbtn);
}

void EquipListWindow::PaintClient(JKDC& dc,JKRect&)
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

void EquipListWindow::EvButtonClick(WORD ctrlid)
{
	JKListBox *lst=(JKListBox *) GetControl(ID_LISTBOX);
	switch(ctrlid) {
		case ID_BUTTON+1:
			if(lst&&lst->CurrentIndex!=-1&&CurrentIndex!=-1) RecDelete(CurrentIndex);
			break;
		case ID_BUTTON+2:
			if(lst&&lst->CurrentIndex!=-1&&CurrentIndex!=-1) SendMessage(ID_RECDVIEW,CurrentIndex);
			break;
		case ID_BUTTON+3:
			if(lst&&lst->CurrentIndex!=-1&&CurrentIndex!=-1) SendControlMessage(ID_RECMODIFY,CurrentIndex);
			break;
		case ID_BUTTON+4:
			//HideWindow();
			SendControlMessage(ID_RECINPUT);
			break;
		case ID_BUTTON+5:
      {
			EquipPrint *prt=new EquipPrint();
			if(prt) {
				prt->CompareBomb=ListBomb;
				prt->Run();
				delete prt;
			}
			}
			break;
    case ID_BUTTON+6:
			SendControlMessage(ID_REFLUSH);
			HideWindow();
			break;
	}
}
void EquipListWindow::EvLBTouchEntry(WORD ctrlid,uint16 index)
{
	switch (ctrlid) {
		case ID_LISTBOX:
			ScrollListBox *slb=(ScrollListBox *)GetControl(ID_LISTBOX);
			HuboBase *hubo=slb->GetHuboBase(index);
			CurrentIndex=hubo->Number;
			break;
	}
}

void EquipListWindow::EvLBSelectEntry(WORD ctrlid,uint16 index)
{
	switch (ctrlid) {
		case ID_LISTBOX:
			ScrollListBox *slb=(ScrollListBox *)GetControl(ID_LISTBOX);
			HuboBase *hubo=slb->GetHuboBase(index);
			CurrentIndex=hubo->Number;
			if(CurrentIndex!=-1) SendMessage(ID_RECDVIEW,CurrentIndex);
			break;
	}
}

ProcessResult EquipListWindow::RespondMessage(JKMSG& msg)
{
	switch (msg.main) {
		case ID_RECDVIEW:
			EqpView *view=new EqpView();
			if(view) {
				view->SetBomb(CurrentIndex);
				view->Run();
				delete view;
			}
		break;
	}
	return JKWindow::RespondMessage(msg);
}

void EquipListWindow::RecDelete(int16 index)
{
	EqiupFileManage *fm=new EqiupFileManage(EQUIPNAME);
	if(fm) {
		fm->Delete(index);
		ReForm();
		delete fm;
	}
}

void EquipListWindow::ReForm()
{
	ScrollListBox *slb=(ScrollListBox *)GetControl(ID_LISTBOX);
	if(slb) {
		CreatListEntry();
		slb->RedrawWindow();
	}
}

void EquipListWindow::CreatListEntry()
{
	int16 i=0,j=0;
	char buf[100];
	EqiupFileManage *fm=new EqiupFileManage(EQUIPNAME);
	ScrollListBox *scrlistbox=(ScrollListBox *) GetControl(ID_LISTBOX);

	BOMB bm;
	scrlistbox->DestroyAll();
	MouseEvents->SetCursorID(5);
	while(fm&&fm->Read(bm,i)&&scrlistbox){
		if(FindBomb(bm)) {
			sprintf(buf," %-6s  %-10s  %-8s %-10s  %-5s %-4s     %-10s",bm.Name,bm.Place,bm.Kind,bm.Dod,bm.Count,bm.Use,bm.HaveYear);
			scrlistbox->Add(j++,new HuboBase(buf,i));
		}
		i++;
	}
	scrlistbox->Add(j,new HuboBase(" *******  µa‹¡ˆa {·³“¡”a.  *********",-1));
	if(fm) delete fm;
}

BOOL EquipListWindow::FindBomb(BOMB& bm)
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

void EquipListWindow::InitListBomb()
{
	*ListBomb.Name=0;
	*ListBomb.Kind=0;
	*ListBomb.Dod=0;
	*ListBomb.Lot=0;
	*ListBomb.Born=0;
	*ListBomb.Place=0;
	*ListBomb.Use=0;
}

