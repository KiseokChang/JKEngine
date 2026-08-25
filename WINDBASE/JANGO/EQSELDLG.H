#ifndef _EQSELDLH_H
#define _EQSELDLH_H

#include "jkdialog.h"


class FileSellectDialog : public JKDialog
{
public:
	FileSellectDialog(JKRect rect);
 ~FileSellectDialog();

	void SetupWindow();
	void EvButtonClick(WORD ctrlid);
	void EvDeadKeyDown(uint16 key,WORD /*modifier*/);
	void EvShowWindow();
};

#endif