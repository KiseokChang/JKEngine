#ifndef _HOWMNDLG_H
#define _HOWMNDLG_H

#include "howizrec.h"
#include "jkdialog.h"


class HowitzerManDialog : public JKDialog
{
public:
	HowitzerManDialog(JKPoint p,JKControl *parent=0);
 ~HowitzerManDialog();

	void InitWindow();
	void SetupWindow();

	void EvShowWindow();
	void EvButtonClick(WORD ctrlid);

	void EvLBTouchEntry(WORD ctrlid,uint16 index);

	void ModifyRecord();
	void DeleteRecord();

	void InitList();
	char *CreateListString(HowitzerRecord& rec);

	char *GetListTitle();
	char *GetListHeader();
	BOOL PrintList(HowitzerManager& dataman);


	HowitzerRecord *InputRecord;
};
#endif