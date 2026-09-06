#ifndef _TGSEARCH_H
#define _TGSEARCH_H

#include "plntgman.h"
#include "tmptgman.h"
#include "jkdialog.h"

class TargetSearchDialog:public JKDialog
{
public:
	TargetSearchDialog(PlanTargetManager& plntgman,TempTargetManager& tmptgman);
 ~TargetSearchDialog();

	void SetupWindow();
	void PaintClient(JKDC& dc,JKRect&);

	void EvShowWindow();

	void EvButtonClick(WORD ctrlid);

	void EvDeadKeyDown(uint16 key,WORD /*modifier*/);

	PlanTargetManager& PlanTargetMan;
	TempTargetManager& TempTargetMan;

};
#endif

