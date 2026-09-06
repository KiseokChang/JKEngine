#ifndef _TGAQ1DLG_H
#define _TGAQ2DLG_H

#include "jkdialog.h"

class TempTarget2Dialog:public JKDialog
{
public:
	TempTarget2Dialog();
 ~TempTarget2Dialog();

	void SetupWindow();
	void EvShowWindow();

	void EvButtonClick(WORD ctrlid);
	void EvCharDown(uint16 key,WORD modifier);
};
#endif

