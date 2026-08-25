#include "jkapp.h"
#include "resbttn.h"
#include "poplist.h"
#include "combobox.h"

Combobox::Combobox(JKRect& rect,WORD ctrlid,WORD bufsize,HuboManager *huboman,BOOL enableedit,JKControl *parent):
	JKEdit(rect,ctrlid,0,bufsize,FALSE,TRUE,parent),
	HuboMan(huboman),Ownership(FALSE)
{
	if(!enableedit) WinAttr|=WA_NOHASFOCUS;
	else WinAttr&=~BYTE(WA_NOHASFOCUS);

	JKRect temp = WinRect.SetLeftTopBy(JKPoint(0,0));
	PopRect = JKRect(temp.left,temp.top+20,temp.right,temp.bottom);
	WinRect.bottom=WinRect.top+19;

	if(!HuboMan){
		HuboMan  = new HuboManager();
		Ownership=TRUE;
	}
	if(!HuboMan) CreationError=TRUE;
}

Combobox::~Combobox()
{
	if(Ownership&&HuboMan) delete HuboMan;
}

JKRect Combobox::CalcClientRect()
{
	JKRect rect=WinRect.SetLeftTopBy(JKPoint(0,0));
	rect.Expand(-1);
	return JKRect(rect.left,rect.top,rect.right-20,rect.bottom);
}

void Combobox::SetupWindow()
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	JKRect temp;
	temp=JKRect(rect.right+1,rect.top,rect.right+18,rect.bottom);
	JKButton *bttn=new JKResButton(temp,ID_BUTTON+1,InternImageID[ComboUp]);
	bttn->WinAttr|=WA_NOINCLIENT | WA_NOHASFOCUS;
	AddControl(bttn);
	PopupListBox *poplist=new PopupListBox(PopRect,ID_LISTBOX+1,HuboMan);
	poplist->WinAttr|=WA_NORESTRICT;
	AddControl(poplist);
}

void Combobox::EvButtonClick(WORD)
{
	SendControlMessage(BUTTON_CLICK);
	SendCommand(SHOWPOPUPLIST);
}

void Combobox::EvCommand(WORD cmdid,WORD,DWORD ,WORD )
{
	if(cmdid==SHOWPOPUPLIST){
		JKControl *ctrl = GetControl(ID_LISTBOX+1);
		if(ctrl) {
			if(Parent) Parent->SetNodeLast(Parent->GetIndex(this));
			ctrl->ShowWindow();
		}
	}
}

void Combobox::EvKeyDown(WORD key,WORD modifier)
{
	if(key!=DOWNARROW) JKEdit::EvKeyDown(key,modifier);
	else EvButtonClick(ID_BUTTON+1);
}

void Combobox::EvCtrlKillFocus(WORD ctrlid)
{
	if(ctrlid==ID_LISTBOX+1) {
		JKControl *ctrl = GetControl(ID_LISTBOX+1);
		if(ctrl) ctrl->HideWindow();
	}
}

void Combobox::EvLBSelectEntry(WORD ctrlid,uint16 index)
{
	PopupListBox *poplist=(PopupListBox *)GetControl(ctrlid);
	if(poplist){
		HuboBase *hubo=(*poplist->HuboMan)[index];
		if(hubo) SetText(hubo->TextBuff);
	}
	SendControlMessage(LB_SELECTENTRY,index);
}

void Combobox::EvLBCharDown(WORD ctrlid,uint16 index,WORD key,WORD /*modifier*/)
{
	if(key==ENTER) EvLBSelectEntry(ctrlid,index);
}