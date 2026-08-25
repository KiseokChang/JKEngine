#ifndef _TYPECTRL_H
#define _TYPECTRL_H
#include "entctrl.h"

class TypeEntryControl : public EntryControl
{
public:
	TypeEntryControl(JKRect& rect,WORD ctlid,EntryBase& ent,BOOL hanmode=FALSE);
 ~TypeEntryControl();
	void SetupWindow();
	void EvSetFocus(DWORD );
	void EvCtrlLeftClick(WORD);
	void EvEditFinish(WORD);
	void EvEditCancel(WORD);
  void EvCtrlKillFocus(WORD);

	void ModifyEntry();
	void ReLoad();

	BOOL StartHanMode;
};

#endif