#ifndef _AIRMNDLG_H
#define _AIRMNDLG_H

#include "airrec.h"
#include "jkdialog.h"


class AirManDialog : public JKDialog
{
public:
	AirManDialog(JKPoint p,JKControl *parent=0);
 ~AirManDialog();

	void InitWindow();
	void SetupWindow();

	void EvShowWindow();
	void EvButtonClick(WORD ctrlid);

	void EvLBTouchEntry(WORD ctrlid,uint16 index);

	void ModifyRecord();
	void DeleteRecord();

	void InitList();
	char *CreateListString(AirRecord& rec);

	char *GetListTitle();
	char *GetListHeader();
	BOOL PrintList(AirManager& airman);

  AirRecord *InputRecord;
};
#endif