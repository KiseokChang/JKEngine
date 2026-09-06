#ifndef _TGAQ2DLG_H
#define _TGAQ2DLG_H

#include "firerec.h"
#include "jkdialog.h"

class TargetAcquisition2Dialog:public JKDialog
{
public:
	TargetAcquisition2Dialog(FireRecord& firerec,BOOL issearch,JKControl *parent=0);
 ~TargetAcquisition2Dialog();

	void SetupWindow();
	void PaintClient(JKDC& dc,JKRect&);

	void EvShowWindow();

	void EvButtonClick(WORD ctrlid);

	void EvDeadKeyDown(uint16 key,WORD /*modifier*/);

	BOOL IsSearch;

	FireRecord& FireRec;
};
#endif

