#ifndef _TGAQ1DLG_H
#define _TGAQ1DLG_H

#include "firerec.h"
#include "jkdialog.h"

class TargetAcquisition1Dialog:public JKDialog
{
public:
	TargetAcquisition1Dialog(FireRecord& firerec,JKControl *parent=0);
 ~TargetAcquisition1Dialog();

	void SetupWindow();
	void PaintClient(JKDC& dc,JKRect&);

	void EvShowWindow();

	void EvButtonClick(WORD ctrlid);

	void EvDeadKeyDown(uint16 key,WORD /*modifier*/);

	FireRecord& FireRec;
};
#endif

