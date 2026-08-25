#ifndef _HPTMNDLG_H
#define _HPTMNDLG_H

#include "hptman.h"
#include "jkwindow.h"


class HPTManDialog : public JKDialog
{
public:
	HPTManDialog();
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

	HPTManager HPTMan;
};
#endif