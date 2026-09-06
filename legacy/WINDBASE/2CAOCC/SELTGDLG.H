#ifndef _SELTGDLG_H
#define _SELTGDLG_H

#include "tgrec.h"
#include "jkdialog.h"

class SelectTargetDialog:public JKDialog
{
public:
	SelectTargetDialog(JKPoint p,int32& index,JKControl *parent=0);
 ~SelectTargetDialog();

	void InitWindow();
	void SetupWindow();
	void PaintClient(JKDC& dc,JKRect&);

	void EvShowWindow();
	void EvButtonClick(WORD ctrlid);

	char *CreateListString(TargetRecord& rec);
	void InitList();
	void InitListCount();

	int32& RefIndex;
};
#endif

