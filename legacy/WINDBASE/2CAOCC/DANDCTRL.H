#ifndef _DANDCTRL_H
#define _DANDCTRL_H

#include "dandent.h"
#include "entctrl.h"

class DandaehoEntryControl : public EntryControl
{
public:
	DandaehoEntryControl(JKRect& rect,WORD ctlid,DandaehoEntry& ent);
 ~DandaehoEntryControl();

	void PaintClient(JKDC& dc,JKRect& );
	void EvLeftClick(JKPoint& );

	void ModifyEntry();
	void ReLoad();

};

#endif
