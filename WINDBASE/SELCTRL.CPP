#include "combobox.h"
#include "selctrl.h"

SelectEntryControl::SelectEntryControl(JKRect& rect,WORD ctlid,EntryBase& ent):
	EntryControl(rect,ctlid,ent)
{
	BackColor=LtGray;
	if(!RefEntry.HuboMan) CreationError=TRUE;
}

SelectEntryControl::~SelectEntryControl()
{}

void SelectEntryControl::SetupWindow()
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.bottom+=75;
	Combobox *combo = new Combobox(rect,ID_EDIT+1,RefEntry.BufferSize,RefEntry.HuboMan);
	combo->SetInsertMode(FALSE);
	combo->TextColor=TextColor;
	combo->BackColor=BackColor;
	combo->SetText(RefEntry.DataBuffer);
	AddControl(combo);
}

void SelectEntryControl::PaintClient(JKDC& dc,JKRect&)
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	dc.SolidBar(rect,BackColor);
}

void SelectEntryControl::ModifyEntry()
{
	JKControl *ctrl=GetControl(ID_EDIT+1);
	if(ctrl){
		ctrl->SendMessage(EDIT_BEGIN);
	}
}


void SelectEntryControl::ReLoad()
{
	JKControl *ctrl=GetControl(ID_EDIT+1);
	if(ctrl){
		int8 *data=new int8[RefEntry.BufferSize+1];
		RefEntry.GetData(data);
		ctrl->SetText(data);
		delete[] data;
    ctrl->RedrawClient();
	}
}

void SelectEntryControl::EvCtrlLeftClick(WORD)
{
	SendControlMessage(ENTRYCTRL_QUERY);
}

void SelectEntryControl::EvLBSelectEntry(WORD ctrlid,uint16)
{
	EvEditFinish(ctrlid);
}

void SelectEntryControl::EvEditFinish(WORD)
{
	JKControl *ctrl=GetControl(ID_EDIT+1);
	if(ctrl){
		if(RefEntry.SetData(ctrl->GetText())){
			ReLoad();
			RedrawWindow();
			SendControlMessage(ENTRYCTRL_ENDMODIFY);
		}
		else{
			ReLoad();
			RedrawWindow();
			SendControlMessage(ENTRYCTRL_INVALIDMODIFY);
		}
	}
}

void SelectEntryControl::EvEditCancel(WORD)
{
	SendControlMessage(ENTRYCTRL_CANCELMODIFY);
}

void SelectEntryControl::EvButtonClick(WORD)
{
	PlaceForward();
}

void SelectEntryControl::EvSetFocus(DWORD )
{
	JKControl *ctrl=GetControl(ID_EDIT+1);
	if(ctrl) ctrl->SetFocus();
}

void SelectEntryControl::EvCtrlKillFocus(WORD ctrlid)
{
	if(ctrlid==ID_EDIT+1) EvEditFinish(ctrlid);
}