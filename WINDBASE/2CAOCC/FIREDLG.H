#ifndef _FIREDLG_H
#define _FIREDLG_H

#include "firerec.h"
#include "jkdialog.h"

class FireDialog:public JKDialog
{
public:
	FireDialog(FireRecord& rec);
 ~FireDialog();

	void SetupWindow();
	void PaintClient(JKDC& dc,JKRect&);

	void EvShowWindow();

	void EvButtonClick(WORD ctrlid);
	void EvDeadKeyDown(uint16 key,WORD /*modifier*/);

	FireRecord& RefRecord;
};
#endif

