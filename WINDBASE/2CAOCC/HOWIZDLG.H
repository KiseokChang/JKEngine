#ifndef _HOWIZDLG_H
#define _HOWIZDLG_H

#include "howizrec.h"
#include "recview.h"
#include "jkdialog.h"

class HowitzerDialog:public JKDialog, public RecordViewBase
{
public:
	HowitzerDialog(JKRect rect,HowitzerRecord& rec,JKControl *parent=0);
 ~HowitzerDialog();

	void SetupWindow();
	void PaintClient(JKDC& dc,JKRect&);

	ProcessResult RespondMessage(JKMSG& msg);
	void EvShowWindow();

	void EvDeadKeyDown(uint16 key,WORD /*modifier*/);

};

#endif

