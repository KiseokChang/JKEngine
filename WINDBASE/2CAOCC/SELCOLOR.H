#ifndef _SELCOLOR_H
#define _SELCOLOR_H

#include "jkdialog.h"

class SelectColorDialog:public JKDialog
{
public:
	SelectColorDialog(JKPoint& pos,BYTE& refcolor,JKControl *parent=0);
 ~SelectColorDialog();
	void SetupWindow();
	void EvButtonClick(WORD /*ctrlid*/);
	void EvLBTouchEntry(WORD /*ctrlid*/,uint16 /*index*/);
  void EvLBSelectEntry(WORD /*ctrlid*/,uint16 /*index*/);

	BYTE& RefColor;
};

#endif
