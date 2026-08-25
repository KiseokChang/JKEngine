#include <ctype.h>
#include "checkbox.h"
#include "chkctrl.h"

CheckEntryControl::CheckEntryControl(JKRect& rect,WORD ctlid,BooleanEntry& ent):
	EntryControl(rect,ctlid,ent)
{}


CheckEntryControl::~CheckEntryControl()
{}

void CheckEntryControl::SetupWindow()
{
	JKRect rect=ClientRect.SetLeftTopBy();
	JKCheckBox *chkbox = new JKCheckBox(rect,ID_CHECKBOX+1);
	chkbox->TextColor=TextColor;
	chkbox->BackColor=BackColor;
	chkbox->IsAutoControl=FALSE;
	BooleanEntry& ent=(BooleanEntry&)RefEntry;
	chkbox->Status=ent.GetBoolean();
	AddControl(chkbox);
}

void CheckEntryControl::ModifyEntry()
{
	BooleanEntry& ent=(BooleanEntry&)RefEntry;
	if(ent.SetBoolean(!ent.DataBoolean)){
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

void CheckEntryControl::ReLoad()
{
	JKCheckBox *chkbox=(JKCheckBox *)GetControl(ID_CHECKBOX+1);
	if(chkbox){
		BooleanEntry& ent=(BooleanEntry&)RefEntry;
		chkbox->Status=ent.GetBoolean();
		chkbox->RedrawClient();
	}
}

void CheckEntryControl::EvCtrlLeftClick(WORD)
{
	SendControlMessage(ENTRYCTRL_QUERY);
}

void CheckEntryControl::EvSetFocus(DWORD )
{
	JKControl *ctrl=GetControl(ID_CHECKBOX+1);
	if(ctrl) ctrl->SetFocus();
}