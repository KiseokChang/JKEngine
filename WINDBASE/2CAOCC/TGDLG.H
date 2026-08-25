#ifndef _TGDLG_H
#define _TGDLG_H

#include "tgrec.h"
#include "jkdialog.h"

class TargetDialog:public JKDialog
{
public:
	TargetDialog(JKPoint p,TargetRecord& rec,JKControl *parent=0);
 ~TargetDialog();

	void SetupWindow();
	void PaintClient(JKDC& dc,JKRect&);

	void EvShowWindow();

	void EvButtonClick(WORD ctrlid);
	void EvDeadKeyDown(uint16 key,WORD /*modifier*/);

	TargetRecord& RefRecord;
};
#endif

