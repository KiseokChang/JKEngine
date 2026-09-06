#ifndef _PLNTGLST_H
#define _PLNTGLST_H

#include "jkdialog.h"

class PlanTargetListWindow:public JKDialog
{
public:
	PlanTargetListWindow(JKPoint p,JKControl *parent=0);
 ~PlanTargetListWindow();

	void InitWindow();
	void SetupWindow();
	void PaintClient(JKDC& dc,JKRect&);
	ProcessResult RespondMessage(JKMSG& msg);

	void EvShowWindow();
	void EvButtonClick(WORD ctrlid);
	void EvLBSelectEntry(WORD ctrlid,uint16 index);

	char *CreateListString(PlanTargetRecord& rec);
	BOOL IsSearchRecord(PlanTargetRecord& rec);
	void InitList();
	void InitListCount();

	void EvAddTarget();
	void EvModifyTarget();
	void EvDeleteTarget();

	char *GetListTitle();
	char *GetListHeader();
	BOOL PrintList(PlanTargetManager& dataman);

	PlanTargetRecord *InputRecord;
};
#endif

