#ifndef _XBATTLST_H
#define _XBATTLST_H

#include "jkdialog.h"

class XBattalionListDialog:public JKDialog
{
public:
	XBattalionListDialog(JKRect rect,JKControl* parent=0);
 ~XBattalionListDialog();

	void InitWindow();
	void SetupWindow();
	void PaintClient(JKDC& dc,JKRect&);
	ProcessResult RespondMessage(JKMSG& msg);

	void EvShowWindow();
	void EvButtonClick(WORD ctrlid);
	void EvLBSelectEntry(WORD ctrlid,uint16 index);

	char *CreateListString(XBattalionRecord& rec);
	BOOL IsSearchRecord(XBattalionRecord& rec);
	void InitList();
	void InitListCount();

	BOOL PreSaveTarget(XBattalionRecord& rec);

	void EvAddTarget();
	void EvModifyTarget();
	void EvDeleteTarget();

	char *GetListTitle();
	char *GetListHeader();
	BOOL PrintList(XBattalionManager& dataman);

	uint16 SearchCount;
	uint16 SearchArray[3];

	XBattalionRecord *CompRecord;
	XBattalionRecord *InputRecord;
};
#endif

