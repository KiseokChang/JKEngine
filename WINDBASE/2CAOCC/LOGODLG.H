#ifndef _LOGODLG_H
#define _LOGODLG_H

#include "jkdialog.h"

class LogoDialog:public JKDialog
{
public:
	LogoDialog(JKRect rect,JKControl *parent=0);
 ~LogoDialog();

	void InitWindow();
	void PaintClient(JKDC& dc,JKRect&);

	void EvShowWindow();
	void EvCharDown(uint16 key,WORD modifier);
	void EvOnlyMove(JKPoint&);
	void EvNcOnlyMove(JKPoint&);

	void EvLeftRelease(JKPoint&);


	void EvTimerHappen(DWORD);
};
#endif

