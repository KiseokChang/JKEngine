#ifndef _HTIMEDLG_H
#define _HTIMEDLG_H

#include "timeent.h"
#include "jkdialog.h"

class HTimeDialog:public JKDialog
{
public:
	HTimeDialog(JKPoint p,JKControl *parent=0);
 ~HTimeDialog();

	void SetupWindow();
	void PaintClient(JKDC& dc,JKRect&);

	void EvShowWindow();
	void EvButtonClick(WORD ctrlid);
	void EvDlgOk();

	TimeEntry *TimeEnt[6];
};
#endif

