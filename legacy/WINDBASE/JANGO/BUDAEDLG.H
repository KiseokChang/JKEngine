#ifndef _BUDAEDLG_H
#define _BUDAEDLG_H

#include "jkdialog.h"


class ChangeBudaeDialog : public JKDialog
{
public:
	ChangeBudaeDialog(JKRect rect);
 ~ChangeBudaeDialog();

	void SetupWindow();
	void EvButtonClick(WORD ctrlid);

	void EvDlgOk();
};

#endif