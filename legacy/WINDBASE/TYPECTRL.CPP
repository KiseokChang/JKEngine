#include "jkedit.h"
#include "typectrl.h"

TypeEntryControl::TypeEntryControl(JKRect& rect,WORD ctlid,EntryBase& ent,BOOL hanmode):
	EntryControl(rect,ctlid,ent),StartHanMode(hanmode)
{}

TypeEntryControl::~TypeEntryControl()
{}

void TypeEntryControl::SetupWindow()
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	JKEdit *edit= new JKEdit(rect,ID_EDIT+1,0,RefEntry.BufferSize,FALSE,StartHanMode);
	edit->SetInsertMode(FALSE);
	edit->TextColor=TextColor;
	edit->BackColor=BackColor;
	edit->SetText(RefEntry.DataBuffer);
	AddControl(edit);
}

void TypeEntryControl::ModifyEntry()
{
	JKControl *ctrl=GetControl(ID_EDIT+1);
	if(ctrl){
		ctrl->SendMessage(EDIT_BEGIN);
	}
}

void TypeEntryControl::ReLoad()
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

void TypeEntryControl::EvCtrlLeftClick(WORD)
{
	SendControlMessage(ENTRYCTRL_QUERY);
}


void TypeEntryControl::EvEditFinish(WORD)
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

void TypeEntryControl::EvEditCancel(WORD)
{
	SendControlMessage(ENTRYCTRL_CANCELMODIFY);
}

void TypeEntryControl::EvSetFocus(DWORD )
{
	JKControl *ctrl=GetControl(ID_EDIT+1);
	if(ctrl) ctrl->SetFocus();
}

void TypeEntryControl::EvCtrlKillFocus(WORD ctrlid)
{
	if(ctrlid==ID_EDIT+1) EvEditFinish(ctrlid);
}
