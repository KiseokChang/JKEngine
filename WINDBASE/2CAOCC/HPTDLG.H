#ifndef _HPTDLG_H
#define _HPTDLG_H

#include "hptrec.h"
#include "recview.h"
#include "jkdialog.h"

class HPTDialog : public JKDialog,public RecordViewBase
{
public:
	HPTDialog(JKPoint p,HPTRecord& recbase,int16 currec);
 ~HPTDialog();

	void SetupWindow();
	void EvShowWindow();
	void PaintClient(JKDC& dc,JKRect&);
	ProcessResult RespondMessage(JKMSG& msg);

	void EvDeadKeyDown(uint16 key,WORD /*modifier*/);

	int16 CurrentIndex;
};
#endif