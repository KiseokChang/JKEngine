#ifndef _CHKCTRL_H
#define _CHKCTRL_H

#include "boolent.h"
#include "entctrl.h"

class CheckEntryControl:public EntryControl
{
public:
	CheckEntryControl(JKRect& rect,WORD ctlid,BooleanEntry& ent);
 ~CheckEntryControl();
	void SetupWindow();
	void EvCtrlLeftClick(WORD);
	void EvSetFocus(DWORD );

	void ModifyEntry();
	void ReLoad();
};
#endif