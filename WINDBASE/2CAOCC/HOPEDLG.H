#ifndef _HOPEDLG_H
#define _HOPEDLG_H

#include "firerec.h"
#include "jkdialog.h"

class YomangAcquisitionDialog:public JKDialog
{
public:
	YomangAcquisitionDialog(FireRecord& firerec);
 ~YomangAcquisitionDialog();

	void SetupWindow();
	void PaintClient(JKDC& dc,JKRect&);

	void EvShowWindow();

	void EvButtonClick(WORD ctrlid);

	void EvDeadKeyDown(uint16 key,WORD /*modifier*/);

	FireRecord& FireRec;
};
#endif

