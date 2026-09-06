#ifndef _ROADCLDLG_H
#define _ROADDLG_H

#include "numentry.h"
#include "firerec.h"
#include "jkdialog.h"

class RoadDialog:public JKDialog
{
public:
	RoadDialog(FireRecord& firerec,JKControl *parent=0);
 ~RoadDialog();

	void SetupWindow();
	void PaintClient(JKDC& dc,JKRect&);

	void EvShowWindow();

	void EvButtonClick(WORD ctrlid);
	void EvDlgOk();

	void EvDeadKeyDown(uint16 key,WORD /*modifier*/);

	FireRecord& FireRec;

	NumberEntry Entry1;
	NumberEntry Entry2;
};
#endif

