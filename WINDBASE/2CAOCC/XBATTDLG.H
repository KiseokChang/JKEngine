#ifndef _XBATTDLG_H
#define _XBATTDLG_H

#include "xbattrec.h"
#include "jkdialog.h"

class XBattalionDialog:public JKDialog
{
public:
	XBattalionDialog(JKPoint p,XBattalionRecord& rec,JKControl *parent=0);
 ~XBattalionDialog();

	void SetupWindow();
	void EvShowWindow();

	void PaintClient(JKDC& dc,JKRect&);

	void EvButtonClick(WORD ctrlid);
	void EvDeadKeyDown(uint16 key,WORD /*modifier*/);

	XBattalionRecord& RefRecord;
};
#endif

