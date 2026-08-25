#ifndef _CIRCLDLG_H
#define _CIRCLDLG_H

#include "numentry.h"
#include "firerec.h"
#include "jkdialog.h"

class CircleDialog:public JKDialog
{
public:
	CircleDialog(FireRecord& firerec,JKControl *parent=0);
 ~CircleDialog();

	void SetupWindow();
	void PaintClient(JKDC& dc,JKRect&);

	void EvShowWindow();

	void EvButtonClick(WORD ctrlid);
	void EvDlgOk();

	void EvDeadKeyDown(uint16 key,WORD /*modifier*/);

	FireRecord& FireRec;

	NumberEntry Entry1;
};
#endif

