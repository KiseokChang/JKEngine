#ifndef _SELCTRL_H
#define _SELCTRL_H
#include "entctrl.h"

class SelectEntryControl:public EntryControl
{
public:
	SelectEntryControl(JKRect& rect,WORD ctlid,EntryBase& ent);
 ~SelectEntryControl();
	void SetupWindow();
	void PaintClient(JKDC& dc,JKRect& paintrect);

	void EvSetFocus(DWORD );
	void ModifyEntry();
	void ReLoad();
	void EvCtrlLeftClick(WORD);
	void EvLBSelectEntry(WORD,uint16);
	void EvEditFinish(WORD);
	void EvEditCancel(WORD);
	void EvCtrlKillFocus(WORD ctrlid);
 	void EvButtonClick(WORD);
};
#endif