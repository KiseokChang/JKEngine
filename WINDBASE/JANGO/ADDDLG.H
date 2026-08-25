#ifndef _FILEDLG_H
#define _FILEDLG_H

#include "huboman.h"
#include "jkdialog.h"

class AddFileDialog: public JKDialog
{
public:
	AddFileDialog();
 ~AddFileDialog();

	void InitWindow();
	void SetupWindow();
	void PaintClient(JKDC& dc,JKRect& paintrect);

	void EvShowWindow();
	void EvButtonClick(WORD ctrlid);
	void EvLBSelectEntry(WORD ctrlid,uint16 index);

	BOOL ExecuteAdd();
	void InitBudae();
	void InitAddMode();

	HuboManager *HuboMan;
	uint16 AddMode;
};
#endif
