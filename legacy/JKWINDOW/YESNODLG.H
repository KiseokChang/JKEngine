#ifndef _YESNODLG_H
#define _YESNODLG_H

#include "jkdialog.h"

class YesNoDialog:public JKDialog
{
public:
	YesNoDialog(JKRect rect,int8 *msgstr,JKControl *parent=0);
 ~YesNoDialog();
	void SetupWindow();
	void EvShowWindow();
	void EvDeadCharDown(WORD key,WORD /*modifier*/);
	void EvButtonClick(WORD ctrlid);
	void EvCharDown(WORD key,WORD modifier);

	int8 *MessageString;
};
#endif