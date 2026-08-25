#include "entctrl.h"

EntryControl::EntryControl(JKRect& rect,WORD ctlid,EntryBase& ent):
	JKControl(rect,ctlid),RefEntry(ent)
{
	BackColor=LtGray;
	WinAttr|=WA_CHANGEPLACE;
	if(!&RefEntry) CreationError=TRUE;
}

ProcessResult EntryControl::RespondMessage(JKMSG& msg)
{
	switch(msg.main){
		case ENTRYCTRL_BEGINMODIFY:
			if(IsShow) ModifyEntry();
			break;
		case ENTRYCTRL_RELOAD:
			ReLoad();
			break;
		default:
			break;
	}
	return JKControl::RespondMessage(msg);
}