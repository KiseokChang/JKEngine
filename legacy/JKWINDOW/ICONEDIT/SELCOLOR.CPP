#include "jkbutton.h"
#include "scrllist.h"
#include "selcolor.h"

BYTE ColorNumber[16]={
	Black,		White,	Blue,			Green,
	Cyan,			Red,		Magenta,	Brown,
	LtBlue,		LtGreen,LtCyan,		LtRed,
	LtMagenta,Yellow,	LtGray,	Gray
};

char *ColorName[16]={
	"Black",		"White",	"Blue",			"Green",
	"Cyan",			"Red",		"Magenta",	"Brown",
	"LtBlue",		"LtGreen","LtCyan",		"LtRed",
	"LtMagenta","Yellow",	"LtGray",		"Gray"
};

SelectColorDialog::SelectColorDialog(JKPoint& pos,BYTE& refcolor,JKControl *parent):
	JKDialog(JKRect(0,0,250,200).OffsetBy(pos),parent),RefColor(refcolor)
{
	SetText("¬‚¬w ¬åÈ‚");
	WinAttr|=WA_TITLEMOVEABLE;
}
SelectColorDialog::~SelectColorDialog()
{}

void SelectColorDialog::SetupWindow()
{
	JKRect rect = ClientRect.SetLeftTopBy();
	rect.Expand(-20);
	JKRect temp=rect;
		temp.right-=80;
	ScrollListBox *scrllist=new ScrollListBox(temp,ID_LISTBOX+1);
	if(scrllist){
		scrllist->DragScrollFlag=TRUE;
		AddControl(scrllist);
		for(int16 i=0;i<16;i++)
			scrllist->Add(scrllist->HuboManager::NodeCount,new HuboBase(ColorName[i],ColorNumber[i]));
		char Buffer[10];
		for(i=14;i<256;i++){
			if(i!=Gray&&i!=LtGray){
				sprintf(Buffer,"%3d",i);
				scrllist->Add(scrllist->HuboManager::NodeCount,new HuboBase(Buffer,i));
			}
		}
		scrllist->SetCurrentIndex(scrllist->GetIndexByNumber(RefColor));
	}
	JKButton *bttn;
	rect.left=rect.right-70;

	temp=rect;
	temp.top=temp.bottom-30;
	bttn=new JKButton(temp,ID_BUTTON+2);
	bttn->SetText("Âá­¡");
	AddControl(bttn);

	temp.bottom=temp.top-10;
	temp.top=temp.bottom-30;
	bttn=new JKButton(temp,ID_BUTTON+1);
	bttn->SetText("ÑÂ·¥");
	AddControl(bttn);

	temp.bottom=temp.top-10;
	temp.top=rect.top;
	bttn=new JKButton(temp,ID_BUTTON+3);
	bttn->Depth=-1;
	bttn->SetEnable(FALSE);
	AddControl(bttn);
}

void SelectColorDialog::EvButtonClick(WORD ctrlid)
{
	if(ctrlid==ID_BUTTON+1){
		ScrollListBox *scrllist = (ScrollListBox *)GetControl(ID_LISTBOX+1);
		if(!scrllist) return;
		int16 index=scrllist->CurrentIndex;
		if(index<0) return;
		EvLBSelectEntry(ID_LISTBOX+1,index);
	}
	else if(ctrlid==ID_BUTTON+2) SendMessage(ID_CANCEL);
}

void SelectColorDialog::EvLBTouchEntry(WORD ctrlid,uint16 index)
{
	if(ctrlid!=ID_LISTBOX+1) return;
	ScrollListBox *scrllist = (ScrollListBox *)GetControl(ID_LISTBOX+1);
	if(!scrllist) return;
	HuboBase *hubo=(*scrllist)[index];
	if(!hubo) return;
	JKControl *ctrl = GetControl(ID_BUTTON+3);
	if(ctrl){
		ctrl->BackColor=hubo->Number;
		ctrl->RedrawClient();
	}
}

void SelectColorDialog::EvLBSelectEntry(WORD ctrlid,uint16 index)
{
	if(ctrlid!=ID_LISTBOX+1) return;
	ScrollListBox *scrllist = (ScrollListBox *)GetControl(ID_LISTBOX+1);
	if(!scrllist) return;
	HuboBase *hubo=(*scrllist)[index];
	if(!hubo) return;
	RefColor=hubo->Number;
	SendMessage(ID_OK);
}
