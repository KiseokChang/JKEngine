#ifndef _TGREFCTL_H
#define _TGREFCTL_H

#include "tgrefent.h"
#include "entctrl.h"

class TargetRefEntryControl : public EntryControl
{
public:
	TargetRefEntryControl(JKPoint& p,WORD ctlid,TargetRefEntry& ent);
 ~TargetRefEntryControl();

	void PaintClient(JKDC& dc,JKRect& );
	void EvLeftClick(JKPoint& );

	void ModifyEntry();
	void ReLoad();

	TargetRecord SelectTarget;
};

#endif
