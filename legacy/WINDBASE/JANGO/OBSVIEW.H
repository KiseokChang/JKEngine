#ifndef _OBSVIEW_H
#define _OBSVIEW_H

#include "dateent.h"
#include "persnman.h"
#include "jkdialog.h"

class ObserveView:public JKDialog
{
public:
	ObserveView(JKRect rect,WORD ctrlid,PersonManager& recman,PersonRecord& rec,int16 currec,JKControl *parent=0);

	void SetupWindow();
	void PaintClient(JKDC& dc,JKRect& paintrect);

	void EvButtonClick(WORD ctrlid);
	void EvKeyDown(uint16 key,WORD modifier);
	void EvDeadKeyDown(uint16 key,WORD modifier);
	void EvCtrlLeftClick(WORD ctrlid);

	void ReLoad();
	BOOL SaveData();
	BOOL SaveChange();
	BOOL PrintObserveData();

	int16 ObserveIndex;
	PersonManager& RecordMan;
	int16 CurrentIndex;
	DateEntry Date;
	PersonRecord& RefRecord;
};
#endif