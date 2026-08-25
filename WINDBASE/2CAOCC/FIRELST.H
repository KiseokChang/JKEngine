#ifndef _FIRELST_H
#define _FIRELST_H

#include "dateent.h"
#include "timeent.h"
#include "fireman.h"
#include "jkdialog.h"

class FireListDialog:public JKDialog
{
public:
	FireListDialog(JKPoint& p,JKControl *parent=0);
 ~FireListDialog();

	void InitWindow();
	void SetupWindow();
	void PaintClient(JKDC& dc,JKRect&);
	ProcessResult RespondMessage(JKMSG& msg);

	void EvShowWindow();
  void EvCheckToggle(WORD /*ctrlid*/,BOOL /*status*/);
	void EvButtonClick(WORD ctrlid);
	void EvLBSelectEntry(WORD ctrlid,uint16 index);
	void EvDeadKeyDown(uint16 key,WORD /*modifier*/);

	char *CreateListString(FireRecord& rec);
	int16 CompareDateTime(FireRecord& rec,DateEntry *dateent,TimeEntry *timeent);
	BOOL IsTimeScope(FireRecord& rec);
	void InitList();
	void InitListCount(BOOL showbalsu=FALSE,uint32 balsu=0);

	void EvAddTarget();
	void EvModifyTarget();
	void EvDeleteTarget();

	BOOL GetListKind();
	char *GetListTitle();
	char *GetListHeader();
	BOOL PrintList(FireManager& dataman);

	FireRecord *CompRecord;
	FireRecord *InputRecord;

	DateEntry MinDate,MaxDate;
	TimeEntry MinTime,MaxTime;
};
#endif

