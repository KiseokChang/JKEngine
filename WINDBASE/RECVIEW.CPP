#include "entctrl.h"
#include "recview.h"

RecordViewBase::RecordViewBase(JKControl& refctrl,RecordBase& refrec):
	RefControl(refctrl),RefRecord(refrec),IsChange(FALSE)
{}

RecordViewBase::~RecordViewBase()
{}

void RecordViewBase::RV_RespondMessage(JKMSG& msg)
{
	uint16 ctrlid=LOWORD(msg.sub);
	if(msg.main==ENTRYCTRL_QUERY)	EvEntryCtrlQuery(ctrlid);
	else if(msg.main==ENTRYCTRL_ENDMODIFY) EvEntryCtrlEndModify(ctrlid);
	else if(msg.main==ENTRYCTRL_CANCELMODIFY)	EvEntryCtrlCancelModify(ctrlid);
	else if(msg.main==ENTRYCTRL_INVALIDMODIFY) EvEntryCtrlInvalidModify(ctrlid);
}

void RecordViewBase::EvEntryCtrlQuery(WORD ctrlid)
{
	ModifyEntry(ctrlid);
}

void RecordViewBase::EvEntryCtrlEndModify(WORD){}

void RecordViewBase::EvEntryCtrlCancelModify(WORD){}

void RecordViewBase::EvEntryCtrlInvalidModify(WORD ctrlid)
{
	JKControl *ctrl=RefControl.GetControl(ctrlid);
	if(ctrl) ctrl->SendMessage(ENTRYCTRL_BEGINMODIFY);
}

BOOL RecordViewBase::ModifyEntry(WORD ctrlid)
{
	JKControl *ctrl=RefControl.GetControl(ctrlid);
	if(ctrl) {
		ctrl->SendMessage(ENTRYCTRL_BEGINMODIFY);
		return TRUE;
	}
	return FALSE;
}


void RecordViewBase::ReLoad()
{
	JKControl *ctrl = RefControl.HeadNode;
	while(ctrl) {
		ctrl->SendMessage(ENTRYCTRL_RELOAD);
		ctrl=(JKControl *)ctrl->next;
	}
	IsChange=FALSE;
}
