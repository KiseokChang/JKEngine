#ifndef _HPTMNDLG_H
#define _HPTMNDLG_H

#include "jkdialog.h"


class HPTManDialog : public JKDialog
{
public:
	HPTManDialog(JKPoint p,HPTRecord& inputrec,JKControl *parent=0);
 ~HPTManDialog();

	void InitWindow();
	void SetupWindow();

	void EvShowWindow();
	void EvButtonClick(WORD ctrlid);

	void EvLBTouchEntry(WORD ctrlid,uint16 index);

	void ModifyRecord();
	void DeleteRecord();

	void InitList();
	char *CreateListString(HPTRecord& rec);

	char *GetListTitle();
	char *GetListHeader();
	BOOL PrintList(HPTManager& dataman);

	HPTRecord& InputRecord;
};
#endif