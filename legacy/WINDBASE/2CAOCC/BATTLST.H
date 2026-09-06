#ifndef _BATTLST_H
#define _BATTLST_H

#include "jkdialog.h"

class BattalionListDialog:public JKDialog
{
public:
	BattalionListDialog(JKRect rect,JKControl *parent=0);
 ~BattalionListDialog();

	void InitWindow();
	void SetupWindow();
	void PaintClient(JKDC& dc,JKRect&);
	ProcessResult RespondMessage(JKMSG& msg);

	void EvShowWindow();
	void EvCheckToggle(WORD /*ctrlid*/,BOOL /*status*/);
	void EvButtonClick(WORD ctrlid);
	void EvLBSelectEntry(WORD ctrlid,uint16 index);

	char *CreateListString(BattalionRecord& rec);
	BOOL IsSearchRecord(BattalionRecord& rec);
	void InitList();
	void InitListCount();

	BOOL PreSaveTarget(BattalionRecord& rec);

	void EvAddTarget();
	void EvModifyTarget();
	void EvDeleteTarget();

  BOOL GetListKind();
	char *GetListTitle();
	char *GetListHeader();
	BOOL PrintList(BattalionManager& dataman);

	uint16 SearchCount;
	uint16 SearchArray[3];

	BattalionRecord *CompRecord;
	BattalionRecord *InputRecord;
};
#endif

