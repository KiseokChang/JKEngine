#ifndef _OKDIALOG_H
#define _OKDIALOG_H

#include "jkdialog.h"

class OKDialog:public JKDialog
{
public:
	OKDialog(JKRect rect,int8 *msgstr,JKControl *parent=0);
 ~OKDialog();

	void SetupWindow();
	void EvShowWindow();

	void EvButtonClick(WORD ctrlid);
	void EvCharDown(WORD key,WORD modifier);

	int8 *MessageString;
};
#endif

