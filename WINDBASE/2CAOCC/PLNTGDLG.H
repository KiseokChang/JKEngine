#ifndef _PLNTGDLG_H
#define _PLNTGDLG_H

#include "plntgrec.h"
#include "jkdialog.h"

class PlanTargetDialog:public JKDialog
{
public:
	PlanTargetDialog(PlanTargetRecord& rec);
 ~PlanTargetDialog();

	void SetupWindow();
	void PaintClient(JKDC& dc,JKRect&);

	void EvShowWindow();

	void EvButtonClick(WORD ctrlid);
	void EvDeadKeyDown(uint16 key,WORD /*modifier*/);

	PlanTargetRecord& RefRecord;
};
#endif

