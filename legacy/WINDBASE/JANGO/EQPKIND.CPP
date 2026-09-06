#include "poplist.h"
#include "eqpkind.h"

EqpKind::EqpKind(JKRect& rect,WORD ctlid,JKControl *parent=0)
	:JKListBox(rect,ctlid,parent)
{
	//InitEquip();
}

EqpKind::~EqpKind()
{
}

void EqpKind::SetupWindow()
{

	PopupListBox *list=new PopupListBox(JKRect(ClientRect.right+2,ClientRect.top+10,ClientRect.right+85,ClientRect.top+166),ID_LISTBOX+1);
	list->WinAttr|=WA_NOINCLIENT ;
	AddControl(list);

	list->Add(0,new HuboBase("HE",0));
	list->Add(1,new HuboBase("WP",1));
	list->Add(2,new HuboBase("ICM",2));
	list->Add(3,new HuboBase("DP-ICM",3));
	list->Add(4,new HuboBase("ILL",4));
	list->Add(5,new HuboBase("HEAT",5));
	list->Add(6,new HuboBase("RAP",6));
	list->Add(7,new HuboBase("RA-FAS",7));
	list->Add(8,new HuboBase("5.56MM",8));

	list=new PopupListBox(JKRect(ClientRect.right+2,ClientRect.bottom-20,ClientRect.right+60,ClientRect.bottom+50),ID_LISTBOX+2);
	list->WinAttr|=WA_NOINCLIENT ;
	AddControl(list);
	list->Add(0,new HuboBase("®…¤i",9));
	list->Add(1,new HuboBase("¯¡Ðe",10));
	list->Add(2,new HuboBase("¸ó‹e",11));
	list->Add(3,new HuboBase("‰ÅÉ·",12));
}

void EqpKind::InitEquip()
{
	Add(0,new HuboBase("Èe¹·"));
	Add(1,new HuboBase("¸w´¢"));
	Add(2,new HuboBase("¯¥‰Å"));
	Add(3,new HuboBase("’A‰Å"));
}


void EqpKind::SendControlMessage(WORD main,WORD data,DWORD detail,WORD option)
{
	JKListBox::SendControlMessage(main,data,detail,option);
	JKControl *win;
	HuboBase *hubo;

	switch(main){
		case LB_TOUCHENTRY:
			break;
		case LB_SELECTENTRY:
			switch(data) {
				case 0:
					win=GetControl(ID_LISTBOX+1);
					if(win)win->ShowWindow();
					break;
				case 1:
					hubo=GetHuboBase(1);
					if(hubo) strcpy(Kind,hubo->TextBuff);
					HideWindow();
					break;
				case 2:
					win=GetControl(ID_LISTBOX+2);
					if(win) win->ShowWindow();
					break;
				case 3:
					hubo=GetHuboBase(3);
					if(hubo) strcpy(Kind,hubo->TextBuff);
					HideWindow();
					break;
			}
	}
}

void EqpKind::EvLBSelectEntry(WORD ctrlid,uint16 index)
{
	HuboBase *hubo;
	PopupListBox *pop;

	switch(ctrlid) {
		case ID_LISTBOX+1:
			pop=(PopupListBox *) GetControl(ID_LISTBOX+1);
			if(pop) {
				hubo=pop->HuboMan->GetHuboBase(index);
				if(hubo) strcpy(Kind,hubo->TextBuff);
				HideWindow();
			}
			break;
		case ID_LISTBOX+2:
			pop=(PopupListBox *) GetControl(ID_LISTBOX+2);
			if(pop) {
				hubo=pop->HuboMan->GetHuboBase(index);
				if(hubo) strcpy(Kind,hubo->TextBuff);
				HideWindow();
			}
			break;
	}
}


