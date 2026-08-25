#ifndef _TGLST_H
#define _TGLST_H

#include "tgrec.h"
#include "jkdialog.h"

class TargetListDialog:public JKDialog
{
public:
	TargetListDialog(JKPoint p,int32& index,JKControl *parent=0);
 ~TargetListDialog();

	void InitWindow();
	void SetupWindow();
	void PaintClient(JKDC& dc,JKRect&);
	ProcessResult RespondMessage(JKMSG& msg);

	void EvShowWindow();
	void EvButtonClick(WORD ctrlid);
	void EvLBSelectEntry(WORD ctrlid,uint16 index);

	char *CreateListString(TargetRecord& rec);
	BOOL IsSearchRecord(TargetRecord& rec);
	void InitList();
	void InitListCount();

	BOOL PreSaveTarget(TargetRecord& rec);

	void EvAddTarget();
	void EvModifyTarget();
	void EvDeleteTarget();

	char *GetListTitle();
	char *GetListHeader();
	BOOL PrintList(TargetManager& dataman);

	uint16 SearchCount;
	uint16 SearchArray[3];

	TargetRecord *CompRecord;
	TargetRecord *InputRecord;

	int32& RefIndex;
};
#endif

