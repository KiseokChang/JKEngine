#ifndef _AIRDLG_H
#define _AIRDLG_H

#include "airrec.h"
#include "recview.h"
#include "jkdialog.h"

class AirDialog : public JKDialog, public RecordViewBase
{
public:
	AirDialog(JKRect rect,AirRecord& recbase,JKControl *parent=0);
 ~AirDialog();

	void SetupWindow();
	void PaintClient(JKDC& dc,JKRect&);
	ProcessResult RespondMessage(JKMSG& msg);
	void EvShowWindow();
	void EvDeadKeyDown(uint16 key,WORD /*modifier*/);

};
#endif