#ifndef _BATTDLG_H
#define _BATTDLG_H

#include "battrec.h"
#include "jkdialog.h"

class BattalionDialog:public JKDialog
{
public:
	BattalionDialog(JKPoint p,BattalionRecord& rec,JKControl *parent=0);
 ~BattalionDialog();

	void SetupWindow();
	void EvShowWindow();

	void PaintClient(JKDC& dc,JKRect&);

	void EvButtonClick(WORD ctrlid);
	void EvDeadKeyDown(uint16 key,WORD /*modifier*/);

	BattalionRecord& RefRecord;
};
#endif

